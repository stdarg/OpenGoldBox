// Unlimited Adventures: playing a design made with the construction kit
// (docs/games/ua.md, Playing a design; docs/GAMES.md, phase 8 step 1). No
// design ships with the kit, so the tests build one from the kit's own area
// templates (GEO.GLB) and game record (GAME.GLB), as the kit's design menu
// does: two dungeon areas with walls, doors and events of the kinds the
// session plays. The kit is read from OPENGOLD_GOLDBOX_DIR/UA. Run with
// --ua-walk to walk every square of every area and play every event and fight.
#include "opengold/campaign_party.h"
#include "opengold/campaign_save.h"
#include "opengold/character_creator.h"
#include "opengold/coin_purse.h"
#include "opengold/combat_demo.h"
#include "opengold/original.h"
#include "opengold/original_combat.h"
#include "opengold/ua_design.h"
#include "opengold/ua_session.h"
#include "test_support.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace
{
using opengold::GoldBoxGame;
using opengold::por::ExplorationCommand;
using opengold::por::MapDirection;
using opengold::por::TourPhase;
using opengold::test::check;
namespace ua = opengold::ua;
using Bytes = std::vector<std::uint8_t>;

// Where an area's chunks' data lie in the kit's 12,962-byte area record:
// FORM, AMOD, then HDR (290 bytes), MAP (576 squares of 6), ENCR (100 events
// of 20) and STRG (the text pool).
constexpr std::size_t header_at = 24, map_at = 322, events_at = 3786, texts_at = 5794;
constexpr std::size_t text_slots = 400;

// Six-bit text: letters 1 to 26, space to '?' as themselves, 0 the end.
Bytes pack_text(std::string_view text)
{
    std::vector<unsigned> codes;
    for (const char c : text)
    {
        codes.push_back(c >= 0x40 ? static_cast<unsigned>(c - 0x40) : static_cast<unsigned>(c));
    }
    codes.push_back(0);
    Bytes packed;
    std::uint32_t bits = 0;
    unsigned held = 0;
    for (const auto code : codes)
    {
        bits = bits << 6 | code;
        held += 6;
        while (held >= 8)
        {
            held -= 8;
            packed.push_back(static_cast<std::uint8_t>(bits >> held));
        }
    }
    if (held > 0)
    {
        packed.push_back(static_cast<std::uint8_t>(bits << (8 - held)));
    }
    return packed;
}

// An area of the test design, made from the kit's template as the area
// editor makes one.
class AreaBuilder
{
  public:
    explicit AreaBuilder(Bytes area) : bytes_(std::move(area))
    {
        rows_ = bytes_[header_at + 2];
        columns_ = bytes_[header_at + 3];
        // Every edge is a blocked wall of the first set's first graphic.
        for (unsigned n = 0; n < columns_; ++n)
        {
            side(n, 0, MapDirection::north, 0xE1);
            side(n, rows_ - 1, MapDirection::south, 0xE1);
        }
        for (unsigned n = 0; n < rows_; ++n)
        {
            side(0, n, MapDirection::west, 0xE1);
            side(columns_ - 1, n, MapDirection::east, 0xE1);
        }
    }

    // Entry point `entry` (1 to 8) at (x, y), facing 0, 2, 4 or 6.
    void entry(unsigned entry, unsigned x, unsigned y, unsigned facing)
    {
        const auto at = header_at + 0xE + 4 * (entry - 1);
        bytes_[at] = static_cast<std::uint8_t>(y);
        bytes_[at + 1] = static_cast<std::uint8_t>(x);
        bytes_[at + 2] = static_cast<std::uint8_t>(facing);
    }

    // The area's name, which the header holds at +76h.
    void name(std::string_view text)
    {
        std::ranges::copy(text, bytes_.begin() + static_cast<std::ptrdiff_t>(header_at + 0x76));
        bytes_[header_at + 0x76 + text.size()] = 0;
    }

    void side(unsigned x, unsigned y, MapDirection direction, std::uint8_t wall)
    {
        bytes_[map_at + (y * columns_ + x) * 6 + opengold::por::index(direction)] = wall;
    }

    // A wall seen from both of its squares.
    void wall(unsigned x, unsigned y, MapDirection direction, std::uint8_t wall)
    {
        side(x, y, direction, wall);
        side(static_cast<unsigned>(static_cast<int>(x) + opengold::por::step_x(direction)),
             static_cast<unsigned>(static_cast<int>(y) + opengold::por::step_y(direction)),
             opengold::por::reversed(direction), wall);
    }

    // Event `number` (1 to 100) on square (x, y), its 20 bytes given from
    // byte 0 on (the rest 0).
    void event(unsigned number, std::vector<unsigned> record,
               std::optional<std::pair<unsigned, unsigned>> square = {})
    {
        record.resize(20);
        for (std::size_t n = 0; n < 20; ++n)
        {
            bytes_[events_at + (number - 1) * 20 + n] = static_cast<std::uint8_t>(record[n]);
        }
        if (square)
        {
            bytes_[map_at + (square->second * columns_ + square->first) * 6 + 4] =
                static_cast<std::uint8_t>(number);
        }
    }

    // The rest encounter of the squares of zone `zone` (the header's four
    // bytes a zone from +2Eh): event `event`, `chance` in 100 every
    // `minutes` of rest.
    void rest_encounter(unsigned zone, unsigned minutes, unsigned event, unsigned chance)
    {
        const auto at = header_at + 0x2E + 4 * zone;
        bytes_[at] = static_cast<std::uint8_t>(minutes & 0xFF);
        bytes_[at + 1] = static_cast<std::uint8_t>(minutes >> 8);
        bytes_[at + 2] = static_cast<std::uint8_t>(event);
        bytes_[at + 3] = static_cast<std::uint8_t>(chance);
    }

    // Step event `slot` (0 to 7, the header's four bytes a slot from +4Eh):
    // event `event` every `steps` steps taken outside the zones in `zones_off`.
    void step_event(unsigned slot, unsigned steps, unsigned event, unsigned zones_off)
    {
        const auto at = header_at + 0x4E + 4 * slot;
        bytes_[at] = static_cast<std::uint8_t>(steps & 0xFF);
        bytes_[at + 1] = static_cast<std::uint8_t>(steps >> 8);
        bytes_[at + 2] = static_cast<std::uint8_t>(event);
        bytes_[at + 3] = static_cast<std::uint8_t>(zones_off);
    }

    // Whether the overhead map shows the area (bit 0 of the header's +7).
    void overhead_map(bool shown)
    {
        auto &flags = bytes_[header_at + 7];
        flags = static_cast<std::uint8_t>(shown ? flags | 1U : flags & ~1U);
    }

    // A text of the area's pool; its slot (1-based).
    unsigned text(std::string words)
    {
        texts_.push_back(std::move(words));
        return static_cast<unsigned>(texts_.size());
    }

    [[nodiscard]] Bytes finish() const
    {
        auto bytes = bytes_;
        Bytes packed;
        for (std::size_t slot = 0; slot < text_slots; ++slot)
        {
            Bytes text = slot < texts_.size() ? pack_text(texts_[slot]) : Bytes{};
            bytes[texts_at + 6 + slot] = static_cast<std::uint8_t>(text.size());
            packed.insert(packed.end(), text.begin(), text.end());
        }
        std::copy(packed.begin(), packed.end(), bytes.begin() + texts_at + 6 + text_slots);
        bytes[texts_at + 4] = static_cast<std::uint8_t>(packed.size() & 0xFF);
        bytes[texts_at + 5] = static_cast<std::uint8_t>(packed.size() >> 8);
        return bytes;
    }

  private:
    Bytes bytes_;
    unsigned rows_{}, columns_{};
    std::vector<std::string> texts_;
};

void write(const std::filesystem::path &path, const Bytes &bytes)
{
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char *>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
    check(file.good(), "The test design's file is written");
}

// Words of an event record: a text slot's two bytes, low first.
unsigned low(unsigned word)
{
    return word & 0xFF;
}

unsigned high(unsigned word)
{
    return word >> 8;
}

// The test design: areas 5 (the hall) and 6 (the cellar), and a game record
// that starts the party in the hall (docs/games/ua.md, The test design),
// written into `folder`.
std::filesystem::path build_test_design(const std::filesystem::path &kit,
                                        const std::filesystem::path &folder)
{
    std::filesystem::remove_all(folder);
    std::filesystem::create_directories(folder);
    const ua::DesignFiles templates(kit, {});
    const auto blank = templates.data("GEO", 5);
    check(blank && blank->size() == 12962, "The kit has its dungeon template");

    AreaBuilder hall(*blank);
    hall.name("THE TEST HALL");
    hall.entry(1, 2, 2, 2);
    // The hall is safe to rest in, and its map is shown.
    hall.rest_encounter(0, 0, 0, 0);
    hall.overhead_map(true);
    hall.entry(2, 12, 11, 0);
    // A wall down the hall between columns 9 and 10, with a locked door at
    // row 5 and a door for the brass key (key 1) at row 12.
    for (unsigned y = 0; y < 16; ++y)
    {
        const std::uint8_t wall = y == 5 ? 0x23 : y == 12 ? 0x63 : 0xE2;
        hall.wall(9, y, MapDirection::east, wall);
    }
    // A pillar of the second set's walls, and an open doorway of the first.
    for (const auto direction :
         {MapDirection::north, MapDirection::east, MapDirection::south, MapDirection::west})
    {
        hall.wall(5, 8, direction, 0x06);
    }
    hall.wall(3, 12, MapDirection::north, 0x03);
    const auto welcome = hall.text("WELCOME TO THE TEST HALL, ^.");
    const auto key = hall.text("YOU FIND A BRASS KEY.");
    const auto chest = hall.text("OPEN THE CHEST?");
    const auto kobolds = hall.text("KOBOLDS ATTACK!");
    const auto fountain = hall.text("A FOUNTAIN.");
    const auto fountain_menu = hall.text("^DRINK ^LEAVE");
    const auto wiser = hall.text("YOU FEEL WISER.");
    const auto dart = hall.text("A DART TRAP!");
    const auto mayor = hall.text("WILL YOU HELP THE MAYOR?");
    const auto thanks = hall.text("THE MAYOR THANKS YOU.");
    const auto elsewhere = hall.text("YOU ARE ELSEWHERE.");
    const auto stairs = hall.text("GO DOWN THE STAIRS?");
    // 1: text, paused (bit 0 of byte 4), chained (byte 3) to 2.
    hall.event(1, {2, 0, 0, 2, 1, 0, 0, 0, low(welcome), high(welcome)}, {{2, 2}});
    // 2: a special item: quest flag 0 (the brass key) set.
    hall.event(2, {38, 0, 0, 0, low(key), high(key), 0, 0, 0});
    // 3: a yes/no question; yes runs 4, a treasure of 50 platinum and 3 gems.
    hall.event(3, {36, 0, 0, 0, low(chest), high(chest), 0, 0, 4, 0}, {{5, 2}});
    // Its items: ITEM.DAT's 17h, identified, and a random scroll.
    hall.event(4, {3, 0, 0, 0, 50, 0, 0, 0x80, 3, 0, 0, 0, 0x17, 0xFF});
    // 5: once only, three kobolds (MONST 1).
    hall.event(5, {1, 1, 0, 0, low(kobolds), high(kobolds), 0, 100, 3, 1}, {{8, 2}});
    // 6: a menu of two buttons: DRINK runs 7 (500 experience), LEAVE nothing.
    hall.event(6,
               {10, 0, 0, 0, low(fountain), high(fountain), 0, 0, low(fountain_menu),
                high(fountain_menu), 7, 0},
               {{2, 5}});
    hall.event(7, {26, 0, 0, 0, low(wiser), high(wiser), 0, 0, 0xF4, 0x01});
    // 8: a dart for one member at random, 1d4, half on a save.
    hall.event(8, {4, 0, 0, 0, low(dart), high(dart), 0, 0x18, 1, 1, 4, 0, 0, 0x40}, {{5, 5}});
    // 9: a quest's first stage (flag 1 at 0), accepted with Yes: 10 sets flag 2 to 7.
    hall.event(9, {35, 0, 0, 0, low(mayor), high(mayor), 0, 1 << 3, 1, 0, 10, 0}, {{8, 5}});
    hall.event(10, {16, 0, 0, 0, 1, 2, 7});
    // 11: once flag 2 is set (condition 1 on flag 2), the mayor's thanks.
    hall.event(11, {2, (1 << 3) | 1, 2, 0, 0, 0, 0, 0, low(thanks), high(thanks)}, {{4, 10}});
    // 12: a teleporter to (7, 13) facing south, with words on arrival.
    hall.event(12, {34, 0, 0, 0, 0, 0, 0, 0x08, 13, 7, low(elsewhere), high(elsewhere)}, {{2, 8}});
    // 13: a chain to 14, which passes an hour.
    hall.event(13, {23, 0, 0, 0, 0, 0, 0, 0, 14}, {{11, 2}});
    hall.event(14, {27, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0});
    // 15: the stairs: asked, to the cellar's entry point 1.
    hall.event(15, {11, 0, 0, 0, low(stairs), high(stairs), 0, 0x20, 0, 0, 0, 0, 1, 1, 6},
               {{12, 12}});
    // 16: one group of two, goblins (MONST 2) or orcs (3), picked at random.
    hall.event(16, {33, 0, 0, 0, 0, 0, 0, 100, 2, 2, 2, 3}, {{13, 8}});
    // 17: two kobolds out of doors (byte 8's bit 20h), on WILDCOM's field.
    hall.event(17, {1, 0, 0, 0, 0, 0, 0, 100, 0x22, 1}, {{6, 13}});
    // 18: two kobolds against the party and, as group 4, a goblin on its
    // side (byte 0Ah's bit 20h); two squares away (byte 0Eh's bits 60h).
    hall.event(18, {1, 0, 0, 0, 0, 0, 0, 100, 2, 1, 0x20, 0, 0, 0, 0x41, 2}, {{3, 14}});
    // 19: a thug (MONST 84) with his items; 20: one who keeps them (byte
    // 0Ch's bit 80h) where the party cannot die (40h).
    hall.event(19, {1, 0, 0, 0, 0, 0, 0, 100, 1, 84}, {{11, 5}});
    hall.event(20, {1, 0, 0, 0, 0, 0, 0, 100, 1, 84, 0, 0, 0xC0}, {{11, 7}});
    // 21: two kobolds starting north and south of the party (byte 10h's
    // bits 20h and 80h), a square away.
    hall.event(21, {1, 0, 0, 0, 0, 0, 0, 100, 2, 1, 0, 0, 0, 0, 0x20, 0, 0xA0}, {{7, 14}});
    // A wall a square west of it, which cuts the distance short that way.
    hall.wall(2, 14, MapDirection::west, 0xE1);
    // 22: two kobolds where no one may cast (byte 0Eh's bit 80h).
    hall.event(22, {1, 0, 0, 0, 0, 0, 0, 100, 2, 1, 0, 0, 0, 0, 0x80}, {{1, 13}});
    write(folder / "GEO005.DAT", hall.finish());

    AreaBuilder cellar(*blank);
    cellar.entry(1, 1, 1, 2);
    const auto damp = cellar.text("THE CELLAR IS DAMP.");
    const auto statue = cellar.text("A STATUE.");
    const auto statue_menu = cellar.text("^TOUCH IT ^PRAY ^LEAVE");
    const auto zap = cellar.text("IT BURNS!");
    // 1: words that need no Return.
    cellar.event(1, {2, 0, 0, 0, 0, 0, 0, 0, low(damp), high(damp)}, {{1, 1}});
    // 2: back up the stairs to the hall's entry point 2.
    cellar.event(2, {11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 5}, {{6, 6}});
    // 3: a list: TOUCH IT burns everyone for 1d2; PRAY gives the selected
    // member 100 experience; LEAVE nothing.
    cellar.event(3,
                 {21, 0, 0, 0, low(statue), high(statue), 0, 0, low(statue_menu), high(statue_menu),
                  4, 5, 0},
                 {{3, 3}});
    cellar.event(4, {4, 0, 0, 0, low(zap), high(zap), 0, 0, 1, 1, 2});
    cellar.event(5, {26, 0, 0, 0, 0, 0, 0, 4, 100});
    // 6: rats break a rest in the cellar, at its first hour.
    const auto rats = cellar.text("RATS SCURRY OVER THE SLEEPERS.");
    cellar.event(6, {2, 0, 0, 0, 0, 0, 0, 0, low(rats), high(rats)});
    cellar.rest_encounter(0, 60, 6, 100);
    cellar.overhead_map(false);
    // 7: a draft every third step taken in the cellar's zone 0 (step event 1).
    const auto draft = cellar.text("A DRAFT CHILLS THE PARTY.");
    cellar.event(7, {2, 0, 0, 0, 0, 0, 0, 0, low(draft), high(draft)});
    cellar.step_event(0, 3, 7, 0);
    write(folder / "GEO006.DAT", cellar.finish());

    // Area 7, the town: a square of each service and test along row 1.
    AreaBuilder town(*blank);
    town.name("THE TEST TOWN");
    town.entry(1, 1, 3, 2);
    const auto hall_words = town.text("THE TRAINING HALL.");
    const auto tale = town.text("THEY SAY THE MAYOR HIDES GOLD.");
    const auto drinks = town.text("^ALE ^WINE ^SPIRITS");
    const auto blessing = town.text("THE PRIESTS BLESS YOU.");
    const auto market = town.text("THE TOWN SQUARE.");
    const auto guide = town.text("THE GUIDE LEADS YOU.");
    const auto arrived = town.text("HERE YOU ARE.");
    const auto joins = town.text("A WARRIOR JOINS YOU.");
    const auto speaks = town.text("THE WARRIOR NODS.");
    const auto leaves = town.text("THE WARRIOR LEAVES.");
    const auto strangers = town.text("STRANGERS APPROACH.");
    const auto greet = town.text("THE STRANGERS GREET YOU.");
    const auto door = town.text("A STUCK DOOR.");
    const auto gives = town.text("THE DOOR GIVES WAY.");
    const auto toll = town.text("A TOLL OF 5 PLATINUM.");
    const auto paid = town.text("THE TOLL IS PAID.");
    const auto ask = town.text("THE PASSWORD?");
    const auto answer = town.text("SWORDFISH");
    const auto pass = town.text("PASS, FRIEND.");
    const auto wrong = town.text("BEGONE.");
    // 1: a hall training fighters (bit 08h) at rate 10, 1,000 platinum.
    town.event(1, {6, 0, 0, 0, low(hall_words), high(hall_words), 0, 0, 0x08, 10}, {{1, 1}});
    // 2: a tavern with drinks (20h) and a brawl (10h), one tale, the brawl
    // event 3: a kobold.
    town.event(2, {7, 0, 0, 0, 0, 0, 0,           0x30,         low(tale), high(tale),
                   0, 0, 0, 0, 0, 0, low(drinks), high(drinks), 3,         0},
               {{2, 1}});
    town.event(3, {1, 0, 0, 0, 0, 0, 0, 100, 1, 1});
    // 4: a shop at rate 10 selling ITEM.DAT 1 to 3 (row 0, bits 0 to 2).
    town.event(4, {8, 0, 0, 0, 0, 0, 10, 0, 0, 0, 0x07}, {{3, 1}});
    // 5: a temple of level 7 at rate 10 taking donations (08h); a tithe of
    // 100 platinum runs 6.
    town.event(5, {9, 0, 0, 0, 7, 10, 0, 8, 100, 0, 0, 0, 6}, {{4, 1}});
    town.event(6, {2, 0, 0, 0, 0, 0, 0, 0, low(blessing), high(blessing)});
    // 7: a vault; 8: an inn.
    town.event(7, {24}, {{5, 1}});
    town.event(8, {29}, {{6, 1}});
    // 9: a small town with every service: temple level 7 (byte 9's top bits)
    // at rate 10 (byte 0Ch), the hall's fighters, the shop at rate 10 selling
    // item 1 (its last mask), a tale for the pub.
    town.event(9, {22,   0,    0,         0,         low(market), high(market), 0,
                   0x3F, 0x08, 0xE0 | 10, low(tale), high(tale),  10,           0,
                   0,    0,    0,         0,         0,           0x01},
               {{7, 1}});
    // 10: a guided tour from (8, 3) facing east (bit 10h, facing 2 in bits 2
    // and 3): forward, turn right, forward; words at steps 0 and 3.
    town.event(10, {12, 0, 0, 0, 3, 8, 3,          0x14,        0x3B,         0,
                    0,  0, 0, 0, 0, 3, low(guide), high(guide), low(arrived), high(arrived)},
               {{8, 1}});
    // 11: the kit's NPC 70h joins at half its Hit Points; 12: its words; 13:
    // it leaves.
    town.event(11, {13, 0, 0, 0, low(joins), high(joins), 0, 0, 0x70, 50}, {{9, 1}});
    town.event(12, {14, 0, 0, 0, 0, 0x70, 0, 0, low(speaks), high(speaks)}, {{10, 1}});
    town.event(13, {32, 0, 0, 0, low(leaves), high(leaves), 0, 0, 0x70}, {{11, 1}});
    // 14: strangers two squares off: Approach closes in, Retreat flees, Fight
    // (3) and Talk (15) only when near; at range 0 they talk.
    town.event(
        14,
        {15, 0, 0,  0, low(strangers), high(strangers), 0, 1, 6, 0x08, 0x0D, 0x1B, 0, 0x1C, 4, 0,
         0,  3, 15, 0},
        {{12, 1}});
    town.event(15, {2, 0, 0, 0, 0, 0, 0, 0, low(greet), high(greet)});
    // 16: who tries Strength (test 2) against 3 (40h), once; success chains
    // to 17, 10 experience.
    town.event(16,
               {18, 0, 0, 0, low(door), high(door), 0, 0x48, 3, 1, 4, 0, 0, low(gives), high(gives),
                0, 17},
               {{13, 1}});
    town.event(17, {26, 0, 0, 0, 0, 0, 0, 0, 10});
    // 18: who pays 5 platinum (currency 3).
    town.event(18,
               {19, 0, 0, 0, low(toll), high(toll), 0, 0x0C, 5, 0, 0, 0, 0, low(paid), high(paid)},
               {{14, 1}});
    // 19: a password, two tries.
    town.event(19, {20,        0,          0,           0,           low(ask),
                    high(ask), 0,          0,           low(answer), high(answer),
                    0,         low(wrong), high(wrong), low(pass),   high(pass),
                    0,         0,          0,           0,           2},
               {{1, 3}});
    write(folder / "GEO007.DAT", town.finish());

    const auto settings = templates.data("GAME", 1);
    check(settings && settings->size() == 388, "The kit has its game record");
    auto game = *settings;
    const std::string_view title = "Test Design";
    std::fill_n(game.begin(), 0x20, 0);
    std::copy(title.begin(), title.end(), game.begin());
    game[0x30] = 5;
    game[0x31] = 1;
    // A new character's start: 20,000 experience, 75 platinum, 2 gems, 1
    // jewel and the first set of equipment.
    const auto put_long = [&game](std::size_t at, std::uint32_t value)
    {
        for (std::size_t n = 0; n < 4; ++n)
        {
            game[at + n] = static_cast<std::uint8_t>(value >> (8 * n));
        }
    };
    put_long(0x20, 20000);
    put_long(0x24, 75);
    put_long(0x28, 2);
    put_long(0x2C, 1);
    game[0x32] = 1;
    write(folder / "GAME001.DAT", game);
    return folder;
}

// Each run builds its design in a folder of its own, as the session test and
// the walk run at once.
// Each check builds its own design, named for it, in the folder it runs in
// (its build's), as the checks and the builds' suites run at once.
std::filesystem::path temporary_design(std::string_view name)
{
    return std::filesystem::current_path() / name;
}

std::optional<std::filesystem::path> kit_folder()
{
    const char *games = std::getenv("OPENGOLD_GOLDBOX_DIR");
    if (!games)
    {
        return std::nullopt;
    }
    const auto folder = std::filesystem::path(games) / "UA";
    return ua::is_kit_folder(folder) ? std::optional(folder) : std::nullopt;
}

// Six human fighters made under Unlimited Adventures' rules.
std::shared_ptr<opengold::CampaignParty> new_party(const opengold::original::Content &content)
{
    std::shared_ptr<const opengold::rules::CharacterRules> creation =
        opengold::original::character_rules(opengold::original::default_aging,
                                            GoldBoxGame::unlimited_adventures);
    auto party = std::make_shared<opengold::CampaignParty>(
        opengold::original::rules_module(content), creation);
    for (unsigned n = 0; n < 6; ++n)
    {
        opengold::CharacterCreator creator(creation, 90 + n);
        creator.select(opengold::rules::CreationField::race, "human");
        creator.select(opengold::rules::CreationField::character_class, "fighter");
        creator.name("FIGHTER " + std::to_string(n + 1));
        for (const bool roll : {false, false, false, true, false, false})
        {
            if (roll)
            {
                creator.roll();
            }
            creator.next();
        }
        (void)party->add_pc(creator.create_character());
    }
    party->select(opengold::PartySlot{0});
    return party;
}

// A long rest, every caster keeping the spells it had.
void rest(opengold::CampaignParty &party)
{
    (void)party.rest();
    while (party.state().spell_rest)
    {
        party.keep_rest_spells(party.state().spell_rest->members.front());
    }
}

// The pending fight played to its end, every turn the computer's.
opengold::rules::Snapshot fight(const ua::UaSession &session,
                                const std::shared_ptr<opengold::CampaignParty> &party,
                                const opengold::original::Content &content)
{
    opengold::CombatDemo combat(opengold::original::rules_module(content));
    combat.campaign_party(party);
    combat.encounter(*session.pending_encounter(), 7);
    for (int command = 0;
         command < 3000 && combat.combat().snapshot().outcome == opengold::rules::Outcome::ongoing;
         ++command)
    {
        const auto &state = combat.combat();
        const auto snapshot = state.snapshot();
        const auto actor = std::ranges::find(snapshot.combatants, snapshot.actor,
                                             &opengold::rules::CombatantView::id);
        const bool member =
            actor != snapshot.combatants.end() && actor->side == opengold::rules::Side::party;
        const auto chosen =
            member ? opengold::choose_quick_command(state, false, opengold::Searching::everyone)
                   : opengold::choose_demo_command(state, opengold::Searching::everyone);
        if (!combat.submit(chosen))
        {
            break;
        }
    }
    return combat.combat().snapshot();
}

// Plays what the square's events ask until they end: the answer named in
// `answers` where one fits, else the first choice (the last after a menu
// came back twice), and every fight.
struct Player
{
    ua::UaSession &session;
    std::shared_ptr<opengold::CampaignParty> party;
    const opengold::original::Content &content;
    std::vector<std::string> answers;
    // What is typed where an event asks; "0" once they run out.
    std::vector<std::string> typed;
    std::vector<std::string> said;
    unsigned fights{}, victories{};

    void play()
    {
        std::map<std::string, unsigned> shown;
        for (int step = 0; step < 200; ++step)
        {
            const auto &s = session.snapshot();
            if (!s.dialogue.empty() && (said.empty() || said.back() != s.dialogue))
            {
                said.push_back(s.dialogue);
            }
            switch (s.phase)
            {
            case TourPhase::awaiting_continue:
            {
                std::size_t pick =
                    ++shown[s.dialogue] > 2 && !s.choices.empty() ? s.choices.size() - 1 : 0;
                if (!answers.empty())
                {
                    const auto found = std::ranges::find(s.choices, answers.front());
                    if (found != s.choices.end())
                    {
                        pick = static_cast<std::size_t>(found - s.choices.begin());
                        answers.erase(answers.begin());
                    }
                }
                check(session.choose(s.continue_ticket, pick), "The answer is taken");
                continue;
            }
            case TourPhase::awaiting_input:
            {
                const auto words = typed.empty() ? std::string("0") : typed.front();
                if (!typed.empty())
                {
                    typed.erase(typed.begin());
                }
                check(session.input(s.continue_ticket, words), "The typed answer is taken");
                continue;
            }
            case TourPhase::combat:
            {
                ++fights;
                const auto result = fight(session, party, content);
                victories += result.outcome == opengold::rules::Outcome::victory ? 1 : 0;
                check(session.resolve_combat(result), "The fight's result is taken");
                continue;
            }
            default:
                return;
            }
        }
        check(false, "An event never ends");
    }

    [[nodiscard]] bool heard(std::string_view words) const
    {
        return std::ranges::any_of(said,
                                   [&](const std::string &line)
                                   {
                                       return line.find(words) != std::string::npos;
                                   });
    }
};

std::size_t colours(const opengold::Image &image)
{
    std::set<std::uint32_t> seen;
    for (std::size_t p = 0; p + 3 < image.rgba.size(); p += 4)
    {
        seen.insert(image.rgba[p] << 16 | image.rgba[p + 1] << 8 | image.rgba[p + 2]);
    }
    return seen.size();
}

void a_character_follows_the_kits_rules()
{
    std::shared_ptr<const opengold::rules::CharacterRules> creation =
        opengold::original::character_rules(opengold::original::default_aging,
                                            GoldBoxGame::unlimited_adventures);
    const auto races = creation->choices(opengold::rules::CreationField::race);
    check(std::ranges::none_of(races,
                               [](const auto &choice)
                               {
                                   return choice.id == "half_orc";
                               }),
          "The kit has no half-orcs");
    const auto classes = creation->choices(opengold::rules::CreationField::character_class);
    check(std::ranges::any_of(classes,
                              [](const auto &choice)
                              {
                                  return choice.id == "paladin";
                              }) &&
              std::ranges::none_of(classes,
                                   [](const auto &choice)
                                   {
                                       return choice.id == "druid";
                                   }),
          "Humans may be paladins; no one is a druid");
    opengold::CharacterCreator creator(creation, 5);
    creator.select(opengold::rules::CreationField::race, "human");
    creator.select(opengold::rules::CreationField::character_class, "fighter");
    creator.name("TESTER");
    for (const bool roll : {false, false, false, true, false, false})
    {
        if (roll)
        {
            creator.roll();
        }
        creator.next();
    }
    const auto sheet = creator.create_character().sheet();
    check(sheet.original && sheet.original->experience == 40001 && sheet.original->platinum == 200,
          "A new character starts with the kit's 40,001 experience and 200 platinum");
    check(sheet.level > 1, "It is trained to the levels its experience reaches");
}

// Unlimited Adventures' experience carries every class on past level 9 (the
// kit's own table), and a ranger past level 8 learns a magic-user spell as
// its ranger level rises, and only then: the kit's training gave one to a
// ranger past 8 whichever class rose (docs/ORIGINAL-RULES.md, Bugs).
void a_ranger_learns_spells_as_a_ranger()
{
    std::shared_ptr<const opengold::rules::CharacterRules> creation =
        opengold::original::character_rules(opengold::original::default_aging,
                                            GoldBoxGame::unlimited_adventures);
    opengold::CampaignParty party(opengold::original::rules_module({}), creation);
    opengold::CharacterCreator creator(creation, 11);
    creator.select(opengold::rules::CreationField::race, "human");
    creator.select(opengold::rules::CreationField::character_class, "ranger");
    creator.name("RANGER");
    for (const bool roll : {false, false, false, true, false, false})
    {
        if (roll)
        {
            creator.roll();
        }
        creator.next();
    }
    const auto id = party.add_pc(creator.create_character());
    constexpr std::size_t ranger_slot = 4;
    const auto ranger_level = [&party, id]
    {
        return party.member(id).character.sheet().original->class_levels[ranger_slot];
    };
    check(ranger_level() == 6, "The kit's 40,001 experience makes a ranger of level 6");
    const auto known_spells = [&party, id]
    {
        return party.member(id).character.sheet().original->known_spells.size();
    };
    // Experience a level at a time, as the rules cap a gain short of two.
    unsigned awards = 0;
    const auto earn = [&party, id, &awards]
    {
        party.award_experience(400000, "ranger-check-" + std::to_string(++awards), id);
    };
    const auto ranger_hall = [&party, id]
    {
        auto choice = party.default_advancement(id);
        choice.training["hall"] = {"ranger"};
        return choice;
    };
    const auto train = [&party, id, &ranger_hall](std::vector<std::string> spells)
    {
        auto choice = ranger_hall();
        choice.spells = std::move(spells);
        party.advance(id, choice);
    };
    const auto offered = [&party, id, &ranger_hall]
    {
        return party.advancement_options(id, ranger_hall()).spells;
    };
    while (ranger_level() < 8)
    {
        earn();
        check(offered().empty(), "A ranger rising to 8 or below learns no magic-user spell");
        train({});
    }
    earn();
    const auto spells = offered();
    check(!spells.empty(), "A ranger rising to 9 learns a first-level magic-user spell");
    const auto known = known_spells();
    train({spells.front().id});
    check(ranger_level() == 9 && known_spells() == known + 1,
          "The ranger's training learns its spell");
    for (int level = 10; level <= 12; ++level)
    {
        earn();
        train({offered().front().id});
    }
    check(ranger_level() == 12 && known_spells() == known + 4,
          "Each ranger level past 8 learns one, and levels go on past 11");
}

// A character made for the design starts as its game record says: its
// experience, money and equipment, not the kit's defaults.
void a_character_starts_as_the_design_says(const std::filesystem::path &kit)
{
    const ua::DesignFiles files(kit,
                                build_test_design(kit, temporary_design("opengold_ua_start.dsn")));
    const auto game = ua::decode_game_settings(*files.data("GAME", 1));
    std::shared_ptr<const opengold::rules::CharacterRules> creation =
        opengold::original::design_character_rules(opengold::original::default_aging,
                                                   ua::starting_stock(*game));
    opengold::CampaignParty party(opengold::original::rules_module(ua::load_design_content(files)),
                                  creation);
    opengold::CharacterCreator creator(creation, 5);
    creator.select(opengold::rules::CreationField::race, "human");
    creator.select(opengold::rules::CreationField::character_class, "fighter");
    creator.name("STARTER");
    for (const bool roll : {false, false, false, true, false, false})
    {
        if (roll)
        {
            creator.roll();
        }
        creator.next();
    }
    const auto character = creator.create_character();
    const auto &sheet = character.sheet();
    check(sheet.original && sheet.original->experience == 20000 && sheet.original->platinum == 75 &&
              creator.starting_platinum() == 75,
          "A new character starts with the design's experience and platinum");
    const auto id = party.add_pc(character);
    ua::outfit_new_character(party, id, files);
    check(party.state().wealth[5] == 2 && party.state().wealth[6] == 1,
          "It brings the design's gems and jewelry");
    const auto items = ua::load_design_items(files);
    const auto &carried = party.member(id).character.inventory().items();
    check(carried.size() == 2, "A fighter starts with the first set's two items");
    check(ua::starting_items(1, sheet) == std::vector<unsigned>{0x17, 0x29} &&
              ua::design_item(items, 0x17, true).has_value(),
          "The first set is the kit's ITEM.DAT items 17h and 29h");
}

void the_design_loads_and_plays(const std::filesystem::path &kit)
{
    const auto design = build_test_design(kit, temporary_design("opengold_ua_test.dsn"));
    const ua::DesignFiles files(kit, design);
    const auto content = ua::load_design_content(files);
    check(content.bestiary.size() == 127 && content.bestiary.contains(ua::monster_key(1)),
          "The kit's 127 monsters fight under the original rules");
    const auto &kobold = content.bestiary.at(ua::monster_key(1));
    check(kobold.thac0 == 20 && kobold.armor_class == 7 && kobold.max_hit_points == 3,
          "A kobold has THAC0 20, Armor Class 7 and 3 Hit Points");
    auto session = ua::UaSession::start(files);
    check(session.area().number == 5, "The design starts in its start area");
    check(session.snapshot().pose.x == 2 && session.snapshot().pose.y == 2 &&
              session.snapshot().pose.facing == MapDirection::east,
          "The party stands on the start area's first entry point");
    // The 3D view: the hall's walls over its backdrop.
    const auto view = session.observe_view();
    check(view.width == 88 && view.height == 88 && colours(view) > 8, "The 3D view draws walls");
    auto party = new_party(content);
    session.campaign_party(party);
    Player player{session, party, content};
    check(session.snapshot().dialogue.find("WELCOME TO THE TEST HALL, FIGHTER 1.") !=
              std::string::npos,
          "The start square's words name the selected member: " + session.snapshot().dialogue);
    player.play();
    check(player.heard("YOU FIND A BRASS KEY.") && session.quest_flag(0) == 1,
          "The chained event gives the brass key");
    check(session.explore(ExplorationCommand::turn_left), "The party turns");
    check(session.observe_view().rgba != view.rgba, "Turning shows another view");
    check(session.explore(ExplorationCommand::turn_right), "The party turns back");
    // Walls stop the party: north of the start is the hall's edge.
    session.place(5, 2, 0, MapDirection::north);
    check(!session.explore(ExplorationCommand::forward), "The edge's wall stops a step");
    // The chest: Yes opens it.
    const auto before = party->state().wealth;
    session.place(5, 4, 2, MapDirection::east);
    check(session.explore(ExplorationCommand::forward), "The party steps to the chest");
    player.answers = {"Yes"};
    player.play();
    check(party->state().wealth[4] == before[4] + 50 && party->state().wealth[5] == before[5] + 3,
          "The chest's 50 platinum and 3 gems go to the party");
    const auto &first = party->member(party->state().slots[0]).character.inventory().items();
    check(first.size() == 2, "The chest's two items go to the first member with room");
    // A trap's saving throw that halves the damage (defence 1).
    opengold::rules::HazardAttack dart;
    dart.automatic = true;
    dart.dice = 1;
    dart.sides = 1;
    dart.damage_bonus = 9;
    dart.original_save = 0;
    dart.original_save_modifier = 15;
    dart.save_dc = 1;
    dart.half_on_save = true;
    bool halved = false;
    for (int n = 0; n < 20; ++n)
    {
        (void)party->rest();
        const auto hit = party->hazard_attack(party->state().slots[1], dart);
        check(hit && hit->damage == (hit->saved ? 5 : 10), "A save halves a trap's damage");
        halved = halved || hit->saved;
    }
    check(halved, "Some saves are made");
    // The kobolds.
    const auto experience = party->member(party->state().slots[0]).experience;
    session.place(5, 8, 2, MapDirection::east);
    session.look();
    player.play();
    check(player.fights == 1 && player.victories == 1, "The party beats the kobolds");
    check(party->member(party->state().slots[0]).experience > experience,
          "The kobolds' experience is shared");
    session.look();
    player.play();
    check(player.fights == 1, "A once-only fight does not come back");
    // The quest: help the mayor; the thanks follow only then.
    session.place(5, 4, 10, MapDirection::north);
    session.look();
    player.play();
    check(!player.heard("THE MAYOR THANKS YOU."), "No thanks before the quest");
    session.place(5, 8, 5, MapDirection::north);
    player.answers = {"Yes"};
    session.look();
    player.play();
    check(session.quest_flag(1) == 1 && session.quest_flag(2) == 7,
          "The quest moves on and its event sets a flag");
    session.place(5, 4, 10, MapDirection::north);
    session.look();
    player.play();
    check(player.heard("THE MAYOR THANKS YOU."), "The thanks come once the flag is set");
    // The teleporter.
    session.place(5, 2, 8, MapDirection::north);
    session.look();
    player.play();
    check(session.snapshot().pose.x == 7 && session.snapshot().pose.y == 13 &&
              session.snapshot().pose.facing == MapDirection::south &&
              player.heard("YOU ARE ELSEWHERE."),
          "The teleporter moves the party within the area");
    // The key's door opens with the brass key; the locked door stays shut.
    session.place(5, 9, 12, MapDirection::east);
    check(session.can_step(MapDirection::east), "The brass key opens its door");
    session.place(5, 9, 5, MapDirection::east);
    check(!session.can_step(MapDirection::east), "A locked door stays shut");
    // Down the stairs to the cellar, and back.
    session.place(5, 12, 12, MapDirection::north);
    player.answers = {"Yes"};
    session.look();
    player.play();
    check(session.area().number == 6 && session.snapshot().pose.x == 1 &&
              session.snapshot().pose.y == 1,
          "The stairs take the party to the cellar's entry point");
    session.place(6, 6, 6, MapDirection::north);
    session.look();
    player.play();
    check(session.area().number == 5 && session.snapshot().pose.x == 12 &&
              session.snapshot().pose.y == 11,
          "The cellar's stairs take it back to the hall's second entry point");
    check(session.diagnostics().empty(), "Every event of the test design is played");
}

// The town's services and tests, each played as the test design sets them.
// Pixels of an image that are not see-through.
std::size_t opaque(const opengold::Image &image)
{
    std::size_t count = 0;
    for (std::size_t at = 3; at < image.rgba.size(); at += 4)
    {
        count += image.rgba[at] != 0 ? 1 : 0;
    }
    return count;
}

std::size_t terrain_count(const opengold::CampaignEncounter &encounter,
                          opengold::rules::Terrain terrain)
{
    return static_cast<std::size_t>(std::ranges::count(encounter.field.geometry.terrain, terrain));
}

// The fight the hall's square (x, y) starts, the party facing `facing`.
opengold::CampaignEncounter hall_fight(const ua::DesignFiles &files,
                                       const opengold::original::Content &content, unsigned x,
                                       unsigned y, MapDirection facing)
{
    auto session = ua::UaSession::start(files);
    auto party = new_party(content);
    session.campaign_party(party);
    Player{session, party, content}.play();
    session.place(5, x, y, facing);
    session.look();
    // Return to the event's words.
    while (session.snapshot().phase == TourPhase::awaiting_continue)
    {
        check(session.choose(session.snapshot().continue_ticket, 0), "The words are read");
    }
    check(session.pending_encounter().has_value(),
          "A fight starts at " + std::to_string(x) + "," + std::to_string(y));
    return *session.pending_encounter();
}

void the_kits_fights_look_as_the_kit_draws_them(const std::filesystem::path &kit)
{
    const auto design = build_test_design(kit, temporary_design("opengold_ua_fights.dsn"));
    const ua::DesignFiles files(kit, design);
    const auto content = ua::load_design_content(files);

    // The kobolds in the hall: its walls as DUNGCOM's tiles, their CPIC icons.
    const auto kobolds = hall_fight(files, content, 8, 2, MapDirection::east);
    check(kobolds.field.tiles.size() == 50 * 25 && kobolds.terrain_art.size() == 25,
          "A dungeon fight's field is the kit's 50 by 25 squares of DUNGCOM's 25 tiles");
    check(std::ranges::all_of(kobolds.terrain_art,
                              [](const opengold::Image &tile)
                              {
                                  return tile.width == 24 && tile.height == 24 && colours(tile) > 1;
                              }),
          "Each tile is a 24-pixel square picture");
    check(terrain_count(kobolds, opengold::rules::Terrain::obstacle) > 0 &&
              terrain_count(kobolds, opengold::rules::Terrain::open) > 0,
          "The walls stand on the field");
    // The party's own square, one of the field's open middle squares.
    check(kobolds.field.geometry.at({23, 12}) == opengold::rules::Terrain::open,
          "The party's square is open floor");
    check(kobolds.art.size() == 3 && kobolds.enemies.size() == 3, "Each kobold has its icon");
    for (const auto &art : kobolds.art)
    {
        check(
            art.image.width == 24 && art.image.height == 24 && opaque(art.image) > 50 &&
                opaque(art.image) < 24 * 24 && art.action && opaque(*art.action) > 50,
            "A kobold's icon is a square of the kit's, see-through round it, with its action pose");
    }
    // Out of doors: WILDCOM's open ground, trees and stones.
    const auto outdoors = hall_fight(files, content, 6, 13, MapDirection::north);
    check(outdoors.terrain_art.size() == 38 && outdoors.field.tiles.size() == 50 * 25,
          "An outdoor fight's field is WILDCOM's 38 tiles");
    check(std::ranges::count(outdoors.field.tiles, 37) > 800 &&
              terrain_count(outdoors, opengold::rules::Terrain::obstacle) > 0 &&
              terrain_count(outdoors, opengold::rules::Terrain::difficult) > 0,
          "Most of it is open ground, with trees and brush");
    check(outdoors.enemies.size() == 2, "Two kobolds meet the party outside");

    // Group 4 fights for the party; the distance is the event's, unless a
    // wall ahead cuts it short.
    const auto allied = hall_fight(files, content, 3, 14, MapDirection::north);
    check(allied.enemies.size() == 2 && allied.allies.size() == 1 &&
              allied.allies.front().definition == ua::monster_key(2) &&
              allied.allies.front().side == opengold::rules::Side::party &&
              opengold::is_ally(allied.allies.front().id),
          "The goblin of group 4 stands with the party");
    check(allied.art.size() == 3, "The ally has its icon too");
    check(allied.distance == 2, "The monsters stand the event's two squares away");
    check(hall_fight(files, content, 3, 14, MapDirection::west).distance == 1,
          "A wall a square ahead brings them within one");

    // Enemies in two quarters: dealt from the east, south first, then north;
    // on the field one stands above the party, one below.
    const auto flanked = hall_fight(files, content, 7, 14, MapDirection::east);
    check(flanked.enemy_quarters ==
              std::vector<MapDirection>{MapDirection::south, MapDirection::north},
          "The kobolds start in the south and north quarters");
    {
        auto party = new_party(content);
        opengold::CombatDemo combat(opengold::original::rules_module(content));
        combat.campaign_party(party);
        combat.encounter(flanked, 7);
        const auto start = combat.combat().snapshot();
        const auto cell_of = [&](opengold::rules::EntityId id)
        {
            return std::ranges::find(start.combatants, id, &opengold::rules::CombatantView::id)
                ->cell;
        };
        const int party_row = cell_of(party->state().slots[0]).y;
        check(cell_of(1000).y > party_row && cell_of(1001).y < party_row,
              "One kobold stands south of the party, the other north");
    }

    // Every monster of the kit has its icon, a square or more.
    const auto colors =
        ua::dungeon_field(files, *ua::decode_area(*files.data("GEO", 5)), 2, 2).palette;
    const ua::CombatIcons icons(files);
    const auto items = ua::load_design_items(files);
    unsigned armed = 0;
    for (const auto &[key, fighter] : content.bestiary)
    {
        const auto number = static_cast<unsigned>(std::stoul(key.substr(key.rfind(':') + 1)));
        const auto name = "Monster " + std::to_string(number);
        const auto icon = icons.icon(number, colors);
        check(icon && icon->ready.width >= 24 && icon->ready.height >= 24 &&
                  opaque(icon->ready) > 0 && opaque(icon->action) > 0,
              name + " has its combat icon");
        // Large by its icon, or by its record's top size bit, which the kit keeps.
        const auto monster = *ua::decode_monster(*files.data("MONST", number));
        check(fighter.large == ((monster.raw[0x82] & 0x80) != 0 ||
                                icon->footprint.width * icon->footprint.height > 1),
              name + "'s size is its icon's");
        // Its items, readied as the kit readies them: two hands at most, and
        // only by a creature of a class that may use them.
        const auto carried = ua::monster_items(monster, items);
        const bool classless = std::ranges::all_of(monster.levels,
                                                   [](std::uint8_t level)
                                                   {
                                                       return level == 0;
                                                   });
        unsigned hands = 0;
        for (const auto &item : carried)
        {
            if (!item.stored.readied())
            {
                continue;
            }
            check(!classless, name + " has no class to ready an item with");
            hands += item.base.hands;
            // A readied weapon's dice are its blows'.
            if (item.base.worn_location == 0 && item.base.small_medium_damage.count != 0)
            {
                ++armed;
                check(fighter.attacks[0].dice == item.base.small_medium_damage.count &&
                          fighter.attacks[0].sides == item.base.small_medium_damage.sides,
                      name + " strikes with its readied weapon's dice");
            }
        }
        check(hands <= 2, name + " readies what two hands hold");
    }
    check(armed > 20, "Many of the kit's monsters fight with their weapons");
}

std::size_t items_carried(const opengold::CampaignParty &party)
{
    std::size_t count = 0;
    for (const auto id : party.state().slots)
    {
        count += id ? party.member(id).character.inventory().items().size() : 0;
    }
    return count;
}

unsigned record_word(const ua::Monster &monster, std::size_t at)
{
    return monster.raw[at] | monster.raw[at + 1] << 8;
}

void the_kits_fights_end_as_the_kit_ends_them(const std::filesystem::path &kit)
{
    const auto design = build_test_design(kit, temporary_design("opengold_ua_spoils.dsn"));
    const ua::DesignFiles files(kit, design);
    const auto content = ua::load_design_content(files);
    auto session = ua::UaSession::start(files);
    auto party = new_party(content);
    session.campaign_party(party);
    Player player{session, party, content};
    player.play();

    // The kobolds' experience and their money's worth (2 a platinum, 250 a
    // gem, 2,200 a jewel), shared among the members standing; a fighter of
    // Strength 16 or more gains 10% more.
    const auto kobold = *ua::decode_monster(*files.data("MONST", 1));
    const unsigned worth = 3 * (record_word(kobold, 0x54) + record_word(kobold, 0x4C) * 2 +
                                record_word(kobold, 0x4E) * 250 + record_word(kobold, 0x50) * 2200);
    const auto first = party->state().slots[0];
    const auto before = party->member(first).experience;
    session.place(5, 8, 2, MapDirection::east);
    session.look();
    player.play();
    check(player.victories == 1, "The party beats the kobolds");
    const auto standing = static_cast<unsigned>(
        std::ranges::count_if(party->state().slots,
                              [&](opengold::MemberId id)
                              {
                                  return id && party->member(id).vitals.hit_points > 0;
                              }));
    const unsigned share = worth / standing;
    const auto strength =
        party->member(first).character.sheet().scores[opengold::rules::Ability::strength];
    check(party->member(first).experience - before == (strength > 15 ? share + share / 10 : share),
          "Each member standing gains its share of the kobolds' worth, as the kit gives it");

    // The thug's items go to the party; the second thug keeps his.
    const auto thug = *ua::decode_monster(*files.data("MONST", 84));
    const auto his_items =
        static_cast<std::size_t>(std::ranges::count_if(std::span(thug.raw).subspan(0x18E, 16),
                                                       [](std::uint8_t number)
                                                       {
                                                           return number != 0;
                                                       }));
    check(his_items > 0, "The kit's thug carries items");
    auto carried = items_carried(*party);
    session.place(5, 11, 5, MapDirection::east);
    session.look();
    player.play();
    check(player.victories == 2 && items_carried(*party) == carried + his_items,
          "The thug's items go to the party");
    carried = items_carried(*party);
    session.place(5, 11, 7, MapDirection::east);
    session.look();
    player.play();
    check(player.victories == 3 && items_carried(*party) == carried,
          "A monster that keeps its treasure leaves none");

    // Where the party cannot die, a lost fight leaves the fallen with 1 Hit
    // Point and the adventure goes on.
    session.place(5, 11, 7, MapDirection::east);
    session.look();
    while (session.snapshot().phase == TourPhase::awaiting_continue)
    {
        check(session.choose(session.snapshot().continue_ticket, 0), "The words are read");
    }
    check(session.pending_encounter().has_value(), "The second thug attacks again");
    opengold::rules::HazardAttack blow;
    blow.automatic = true;
    blow.damage_bonus = party->member(first).vitals.hit_points;
    (void)party->hazard_attack(first, blow);
    check(party->member(first).vitals.hit_points <= 0 && !party->member(first).vitals.dead,
          "A member is down");
    opengold::rules::Snapshot lost;
    lost.outcome = opengold::rules::Outcome::defeat;
    check(session.resolve_combat(lost), "The lost fight's result is taken");
    check(party->member(first).vitals.hit_points == 1 &&
              session.snapshot().phase != TourPhase::defeated,
          "The fallen get up with 1 Hit Point and the adventure goes on");
}

void the_town_serves_the_party(const std::filesystem::path &kit)
{
    const ua::DesignFiles files(kit,
                                build_test_design(kit, temporary_design("opengold_ua_town.dsn")));
    const auto content = ua::load_design_content(files);
    auto session = ua::UaSession::start(files);
    auto party = new_party(content);
    session.campaign_party(party);
    Player player{session, party, content};
    player.play();
    const auto first = party->state().slots[0];
    const auto visit = [&](unsigned x, unsigned y, std::vector<std::string> answers,
                           std::vector<std::string> typed = {})
    {
        session.place(7, x, y, MapDirection::north);
        player.answers = std::move(answers);
        player.typed = std::move(typed);
        player.said.clear();
        session.look();
        player.play();
    };
    // The hall trains a fighter with the experience for a level, for 1,000
    // platinum.
    party->grant_script_experience(300000, first);
    party->add_coins(opengold::Coin::platinum, 2000);
    const auto level = party->member(first).character.sheet().level;
    const auto money = opengold::gold_value(party->state().wealth, party->coin_rates());
    visit(1, 1, {"Yes", "FIGHTER 1"});
    check(player.heard("Congratulations... FIGHTER 1 has trained.") &&
              party->member(first).character.sheet().level == level + 1,
          "The hall trains a level");
    check(opengold::gold_value(party->state().wealth, party->coin_rates()) ==
              money - 1000 * party->coin_rates().worth[4] / party->coin_rates().worth[3],
          "Training costs 1,000 platinum");
    visit(2, 1, {"Talk"});
    check(player.heard("THEY SAY THE MAYOR HIDES GOLD."), "The tavern tells its tale");
    visit(2, 1, {"Fight"});
    check(player.fights == 1, "The brawl is the tavern's fight");
    const auto carried = party->member(first).character.inventory().items().size();
    visit(3, 1, {"Buy"});
    check(party->member(first).character.inventory().items().size() == carried + 1 &&
              player.heard("FIGHTER 1 buys"),
          "The shop sells an item");
    visit(4, 1, {"Donate"}, {"100"});
    check(player.heard("Thank you for your generous donation.") &&
              player.heard("THE PRIESTS BLESS YOU."),
          "A tithe of 100 platinum runs the temple's event");
    const auto purse = party->state().wealth;
    visit(5, 1, {"Money", "Exit"});
    check(party->state().wealth[4] == 0 && party->state().wealth[5] == 0, "The vault keeps money");
    visit(5, 1, {"Take", "Exit"});
    check(party->state().wealth == purse, "The vault gives it back");
    visit(6, 1, {"Rest"});
    check(player.heard("The party rests."), "The inn lets the party rest");
    visit(7, 1, {"Pub", "Talk", "Leave", "Leave"});
    check(player.heard("THE TOWN SQUARE.") && player.heard("The party enters a tavern.") &&
              player.heard("THEY SAY THE MAYOR HIDES GOLD."),
          "The small town has a pub");
    visit(8, 1, {});
    check(session.snapshot().pose.x == 9 && session.snapshot().pose.y == 4 &&
              session.snapshot().pose.facing == MapDirection::south &&
              player.heard("THE GUIDE LEADS YOU.") && player.heard("HERE YOU ARE."),
          "The guide leads the party along its path");
    visit(9, 1, {});
    check(player.heard("A WARRIOR JOINS YOU.") && party->state().slots[6], "An NPC joins");
    visit(10, 1, {});
    check(player.heard("THE WARRIOR NODS."), "The NPC's words are heard with it in the party");
    visit(11, 1, {});
    check(player.heard("THE WARRIOR LEAVES.") && !party->state().slots[6], "The NPC leaves");
    visit(10, 1, {});
    check(!player.heard("THE WARRIOR NODS."), "Its words are not heard without it");
    visit(12, 1, {"Approach", "Talk"});
    check(player.heard("THE STRANGERS GREET YOU."), "Approached, the strangers talk");
    const auto experience = party->member(first).experience;
    visit(13, 1, {"FIGHTER 1"});
    check(player.heard("THE DOOR GIVES WAY.") && party->member(first).experience > experience,
          "A strong member opens the door, and its success chains on");
    const auto toll = party->state().wealth;
    visit(14, 1, {"FIGHTER 1"});
    check(player.heard("THE TOLL IS PAID.") && party->state().wealth != toll, "The toll is paid");
    visit(1, 3, {}, {"swordfish"});
    check(player.heard("PASS, FRIEND."), "The password is taken in any case");
    visit(1, 3, {}, {"FISH", "TUNA"});
    check(player.heard("Wrong, try again.") && player.heard("BEGONE."), "Two wrong answers fail");
    check(session.diagnostics().empty(), "Every event of the town is played");
}

// The game's exploration view plays a design through ExplorationSession, as
// it plays a DAX campaign (docs/games/ua.md, The seam).
void the_view_plays_it_through_the_seam(const std::filesystem::path &kit)
{
    const ua::DesignFiles files(kit,
                                build_test_design(kit, temporary_design("opengold_ua_seam.dsn")));
    const auto content = ua::load_design_content(files);
    auto design = std::make_unique<ua::UaSession>(ua::UaSession::start(files));
    opengold::ExplorationSession &session = *design;
    // Return to every text, as the view's Continue gives it.
    const auto read_on = [&session]
    {
        while (session.snapshot().phase == TourPhase::awaiting_continue)
        {
            check(session.choose(session.snapshot().continue_ticket, 0),
                  "Return goes on from words");
        }
    };
    check(ua::is_design_folder(files.design()) && !ua::is_design_folder(kit),
          "A design's folder is named NAME.DSN");
    check(session.dax() == nullptr, "A design has none of a DAX campaign's features");
    check(session.snapshot().tour_finished, "A design has no guided opening");
    check(session.area_name() == "THE TEST HALL",
          "The area's name is the designer's: " + session.area_name());
    const auto view = session.view_picture();
    check(view && view->width == 88, "The window shows the dungeon's 3D view");
    check(session.can_leave(), "Nothing waits before the party arrives");
    const auto party = new_party(content);
    session.campaign_party(party);
    check(!session.can_leave() && session.snapshot().phase == TourPhase::awaiting_continue,
          "The welcome waits for Return");
    read_on();
    // A fight the view cannot set up stops the adventure, saying why.
    design->place(5, 8, 2, MapDirection::east);
    design->look();
    read_on();
    check(session.snapshot().phase == TourPhase::combat && session.pending_encounter(),
          "The kobolds attack");
    check(session.reject_combat("No battlefield") &&
              session.snapshot().phase == TourPhase::faulted &&
              session.snapshot().diagnostic == "No battlefield" && !session.pending_encounter(),
          "A refused fight stops the adventure with its reason");
    // Restart: the design from its start, its welcome again.
    session.restart();
    session.campaign_party(party);
    check(session.snapshot().pose.x == 2 && session.snapshot().pose.y == 2 &&
              session.snapshot().dialogue.find("WELCOME TO THE TEST HALL") != std::string::npos,
          "Restart begins the design again");
    read_on();
    // The hall's overhead map: its walls, its doors, and where the party has been.
    const auto *map = session.area_map();
    check(map && map->at(2, 0).walls[0] && map->at(9, 5).doors[1] && !map->at(9, 5).walls[1] &&
              session.snapshot().seen.test(2 * 16 + 2),
          "The hall's overhead map shows its edge, its locked door and the start");
    design->place(6, 2, 2, MapDirection::north);
    check(session.area_map() == nullptr, "The cellar's designer hid its map");
    // The overland areas have no 3D view yet: nothing to draw, nothing thrown.
    design->place(1, 0, 0, MapDirection::north);
    check(!session.view_picture(), "An overland area shows no 3D view yet");
}

// The kit's monster effects as the fight's (docs/games/ua.md, Monsters'
// effects), a "no magic" fight, and the cellar's step event.
void the_kits_monsters_and_events_play_as_the_kit_has_them(const std::filesystem::path &kit)
{
    const ua::DesignFiles files(
        kit, build_test_design(kit, temporary_design("opengold_ua_effects.dsn")));
    const auto items = ua::load_design_items(files);
    const auto effects = [&](unsigned number)
    {
        const auto monster = *ua::decode_monster(*files.data("MONST", number));
        std::vector<int> codes;
        for (const auto &effect : ua::monster_combatant(monster, items, {1, 1}).effects)
        {
            codes.push_back(effect.code);
        }
        return codes;
    };
    const auto has = [](const std::vector<int> &codes, int code)
    {
        return std::ranges::find(codes, code) != codes.end();
    };
    // The skeleton: half damage from edged weapons (Pool's 115), no fear
    // (Secret's 492) and no cold (Curse's 310).
    const auto skeleton = effects(18);
    check(has(skeleton, 115) && has(skeleton, 492) && has(skeleton, 310),
          "The kit's skeleton fights with its effects");
    // The troll's return and regeneration are Pool of Radiance's own.
    const auto troll = effects(9);
    check(has(troll, 100) && has(troll, 101), "The kit's troll returns and regenerates");
    // The rakshasa's spell immunity has no handler yet: numbered past the kit's own.
    check(has(effects(71), 40 + opengold::original::kit_effect_offset),
          "An effect no game plays yet is kept, unplayed");
    // The no-magic fight.
    const auto content = ua::load_design_content(files);
    auto session = ua::UaSession::start(files);
    auto party = new_party(content);
    session.campaign_party(party);
    Player player{session, party, content};
    player.play();
    session.place(5, 1, 13, MapDirection::north);
    session.look();
    check(session.pending_encounter() && session.pending_encounter()->no_magic,
          "The fight's event bars magic");
    player.play();
    session.place(5, 8, 2, MapDirection::east);
    session.look();
    while (session.snapshot().phase == TourPhase::awaiting_continue)
    {
        check(session.choose(session.snapshot().continue_ticket, 0), "Return goes on");
    }
    check(session.pending_encounter() && !session.pending_encounter()->no_magic,
          "Other fights allow it");
    player.play();
    // The cellar's draft: every third step.
    session.place(6, 1, 4, MapDirection::east);
    player.said.clear();
    for (int step = 0; step < 3; ++step)
    {
        check(!player.heard("A DRAFT CHILLS THE PARTY."), "The draft waits for its third step");
        check(session.explore(ExplorationCommand::forward), "The party walks the cellar");
        player.play();
    }
    check(player.heard("A DRAFT CHILLS THE PARTY."), "The third step brings the draft");
}

// Camping (docs/games/ua.md, Camping): a rest where no encounter waits heals
// as the rules rest; the cellar's rats break one at its first hour, and their
// event runs.
void the_party_camps(const std::filesystem::path &kit)
{
    const ua::DesignFiles files(kit,
                                build_test_design(kit, temporary_design("opengold_ua_camp.dsn")));
    const auto content = ua::load_design_content(files);
    auto session = ua::UaSession::start(files);
    auto party = new_party(content);
    session.campaign_party(party);
    Player player{session, party, content};
    player.play();
    opengold::ExplorationSession &exploring = session;
    session.place(5, 4, 9, MapDirection::north);
    auto state = party->checkpoint();
    for (auto &member : state.roster)
    {
        member.vitals.hit_points = 1;
    }
    party->restore(std::move(state));
    const auto before = party->state().time_minutes;
    check(exploring.camp(opengold::RestKind::long_rest), "The party camps in the hall");
    const auto rest_minutes = party->rule_module().long_rest_policy().duration_minutes;
    check(session.snapshot().phase == TourPhase::completed &&
              session.snapshot().dialogue == "The party rests." &&
              party->state().time_minutes == before + rest_minutes,
          "An unbroken rest takes the rules' time: " + session.snapshot().dialogue);
    const auto &first = party->member(party->state().slots[0]);
    check(first.vitals.hit_points > 1, "The rest heals");
    while (party->state().spell_rest)
    {
        party->keep_rest_spells(party->state().spell_rest->members.front());
    }
    // The cellar's rats.
    session.place(6, 2, 2, MapDirection::north);
    const auto resting = party->state().time_minutes;
    check(exploring.camp(opengold::RestKind::long_rest) &&
              session.snapshot().phase == TourPhase::awaiting_continue &&
              session.snapshot().dialogue == "The party's repose is interrupted!" &&
              party->state().time_minutes == resting + 60,
          "The rats break the rest at its first hour: " + session.snapshot().dialogue);
    check(!exploring.camp(opengold::RestKind::long_rest), "No camp while words wait");
    player.said.clear();
    player.play();
    check(player.heard("RATS SCURRY OVER THE SLEEPERS.") &&
              session.snapshot().phase == TourPhase::completed,
          "The rest encounter's event runs");
}

// Saving and loading (docs/games/ua.md, Saving and loading a game): a game
// saved mid-design loads into a session that plays on as the first does.
void a_game_saves_and_loads(const std::filesystem::path &kit)
{
    const auto design = build_test_design(kit, temporary_design("opengold_ua_save.dsn"));
    const ua::DesignFiles files(kit, design);
    const auto content = ua::load_design_content(files);
    auto first = ua::UaSession::start(files);
    auto party = new_party(content);
    first.campaign_party(party);
    Player player{first, party, content};
    player.play();
    // The kobolds beaten, once only, and the mayor's quest taken.
    first.place(5, 8, 2, MapDirection::east);
    first.look();
    player.play();
    first.place(5, 8, 5, MapDirection::north);
    player.answers = {"Yes"};
    first.look();
    player.play();
    // Nothing is saved while an event waits: the chest asks.
    first.place(5, 4, 2, MapDirection::east);
    check(first.explore(ExplorationCommand::forward) &&
              first.snapshot().phase == TourPhase::awaiting_continue,
          "The chest asks");
    bool refused = false;
    try
    {
        (void)first.saved_state();
    }
    catch (const std::logic_error &)
    {
        refused = true;
    }
    check(refused, "A design is not saved while an event waits");
    player.answers = {"Yes"};
    player.play();
    first.place(5, 4, 9, MapDirection::north);
    check(first.explore(ExplorationCommand::turn_right), "The party turns");
    const opengold::DesignSave saved_design{design.string(), first.saved_state()};
    // A save names the kit's items it was made with, as a DAX save its archives.
    const auto assets = opengold::campaign_asset_identity(kit);
    check(assets.starts_with("UA:ITEMS.DAT:"), "The kit's items identify its saves: " + assets);
    const auto bytes = opengold::encode_campaign(*party, nullptr, assets,
                                                 GoldBoxGame::unlimited_adventures, &saved_design);
    check(opengold::saved_campaign_rules(bytes).design_folder == design.string(),
          "The save's header names its design's folder");
    bool older = false;
    try
    {
        (void)opengold::encode_campaign_in_format(*party, nullptr, assets, 50,
                                                  GoldBoxGame::unlimited_adventures, &saved_design);
    }
    catch (const std::runtime_error &)
    {
        older = true;
    }
    check(older, "An older campaign format cannot record a design");
    auto loaded = opengold::decode_campaign(bytes, *party->character_rules(), party->rule_module(),
                                            assets, nullptr);
    check(loaded.game == GoldBoxGame::unlimited_adventures && loaded.design && !loaded.town &&
              loaded.design->folder == design.string(),
          "The save loads as the design's");
    auto second_party = std::make_shared<opengold::CampaignParty>(
        opengold::original::rules_module(content), party->character_rules());
    second_party->restore(std::move(loaded.party));
    auto second = ua::UaSession::resume(files, loaded.design->state);
    second.campaign_party(second_party);
    check(second.saved_state() == first.saved_state(), "The loaded design is the saved one");
    check(second.snapshot().pose == first.snapshot().pose && second.area().number == 5 &&
              second.snapshot().phase == TourPhase::completed && second.snapshot().dialogue.empty(),
          "The party stands where it was saved, and no event runs again");
    for (unsigned flag = 0; flag < 256; ++flag)
    {
        check(second.quest_flag(flag) == first.quest_flag(flag), "The quest flags are kept");
    }
    check(second_party->state().time_minutes == party->state().time_minutes &&
              second_party->state().wealth == party->state().wealth,
          "The party's clock and money are kept");
    // Both play on alike: the kobolds stay beaten, the mayor thanks the party,
    // and the same steps meet the same events.
    Player again{second, second_party, content};
    for (auto *session : {&first, &second})
    {
        session->place(5, 8, 2, MapDirection::east);
        session->look();
    }
    player.play();
    again.play();
    check(player.fights == 1 && again.fights == 0,
          "A once-only fight done before the save stays done");
    const std::vector<ExplorationCommand> walk{
        ExplorationCommand::turn_left,  ExplorationCommand::forward, ExplorationCommand::forward,
        ExplorationCommand::turn_right, ExplorationCommand::forward, ExplorationCommand::look};
    for (auto *session : {&first, &second})
    {
        session->place(5, 4, 10, MapDirection::north);
        session->look();
    }
    player.said.clear();
    player.play();
    again.play();
    check(again.heard("THE MAYOR THANKS YOU.") && player.said == again.said,
          "The mayor thanks the loaded party for the quest taken before the save");
    for (const auto command : walk)
    {
        player.said.clear();
        again.said.clear();
        check(first.explore(command) == second.explore(command), "A step goes alike");
        player.play();
        again.play();
        check(first.snapshot().pose == second.snapshot().pose && player.said == again.said,
              "The loaded design plays on as the saved one");
    }
    // A state this build cannot read is refused, not half-loaded.
    bool unreadable = false;
    try
    {
        (void)ua::UaSession::resume(files, "UA-STATE 1\n99 0 0 0\n");
    }
    catch (const std::runtime_error &)
    {
        unreadable = true;
    }
    check(unreadable, "A saved state of an area the design lacks is refused");
}

// The walk (plan phase 8, step 1): every square of every area of the design,
// stood on facing each way, its view drawn, its event played, and every
// open step from it taken.
bool walk_every_square(const std::filesystem::path &kit)
{
    const auto design = build_test_design(kit, temporary_design("opengold_ua_walk.dsn"));
    const ua::DesignFiles files(kit, design);
    const auto content = ua::load_design_content(files);
    auto session = ua::UaSession::start(files);
    auto party = new_party(content);
    session.campaign_party(party);
    Player player{session, party, content};
    player.play();
    std::vector<unsigned> areas;
    for (const auto &entry : std::filesystem::directory_iterator(design))
    {
        const auto name = entry.path().filename().string();
        if (name.starts_with("GEO") && name.ends_with(".DAT"))
        {
            areas.push_back(static_cast<unsigned>(std::stoul(name.substr(3, 3))));
        }
    }
    std::ranges::sort(areas);
    unsigned squares = 0, steps = 0;
    for (const auto area : areas)
    {
        session.place(area, 0, 0, MapDirection::north);
        const unsigned rows = session.area().area.rows, columns = session.area().area.columns;
        for (unsigned y = 0; y < rows; ++y)
        {
            for (unsigned x = 0; x < columns; ++x)
            {
                ++squares;
                for (const auto facing : {MapDirection::north, MapDirection::east,
                                          MapDirection::south, MapDirection::west})
                {
                    session.place(area, x, y, facing);
                    check(colours(session.observe_view()) > 2, "The view draws");
                    if (facing == MapDirection::north)
                    {
                        session.look();
                        player.play();
                        session.place(area, x, y, facing);
                    }
                    if (session.can_step(facing))
                    {
                        ++steps;
                        check(session.explore(ExplorationCommand::forward),
                              "An open step is taken");
                        player.play();
                    }
                    if (session.snapshot().phase == TourPhase::defeated)
                    {
                        std::cout << "The party was lost at area " << area << ", " << x << "," << y
                                  << '\n';
                        return false;
                    }
                    rest(*party);
                }
            }
        }
    }
    std::cout << "Walked " << squares << " squares of " << areas.size() << " areas, took " << steps
              << " steps, played " << player.fights << " fights (" << player.victories
              << " won).\n";
    for (const auto &need : session.diagnostics())
    {
        std::cout << "  needs: " << need << '\n';
    }
    return session.diagnostics().empty() && player.fights > 0;
}

} // namespace

int main(int argc, char **argv)
{
    try
    {
        a_character_follows_the_kits_rules();
        a_ranger_learns_spells_as_a_ranger();
        const auto kit = kit_folder();
        if (!kit)
        {
            std::cout << "Skipped: the kit's files are not in OPENGOLD_GOLDBOX_DIR/UA.\n";
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--ua-walk")
        {
            return walk_every_square(*kit) ? 0 : 1;
        }
        a_character_starts_as_the_design_says(*kit);
        the_design_loads_and_plays(*kit);
        the_kits_fights_look_as_the_kit_draws_them(*kit);
        the_kits_fights_end_as_the_kit_ends_them(*kit);
        the_town_serves_the_party(*kit);
        the_view_plays_it_through_the_seam(*kit);
        a_game_saves_and_loads(*kit);
        the_party_camps(*kit);
        the_kits_monsters_and_events_play_as_the_kit_has_them(*kit);
        // The game's startup check plays the test design from the menu (a
        // CTest fixture): it is written where the check looks for it.
        if (const char *fixture = std::getenv("OPENGOLD_UA_DESIGN_FIXTURE"))
        {
            build_test_design(*kit, fixture);
        }
    }
    catch (const std::exception &error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Unlimited Adventures' design plays.\n";
    return 0;
}
