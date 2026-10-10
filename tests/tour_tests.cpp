#include "opengold/rolf_tour.h"
#include "opengold/exploration_view.h"
#include "opengold/npc_portraits.h"
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <queue>
#include <string_view>

using namespace opengold::por;

namespace
{
// A session drives a shared party, so copying is explicit (detached_copy)
// and moving is free (Effective C++ Item 6).
static_assert(!std::is_copy_constructible_v<RolfTourSession> &&
              !std::is_copy_assignable_v<RolfTourSession> &&
              std::is_move_constructible_v<RolfTourSession>);

using Bytes = std::vector<std::uint8_t>;

void check(bool ok, const char *message)
{
    if (!ok)
        throw std::runtime_error(message);
}

std::shared_ptr<const EclProgram> program(Bytes body)
{
    Bytes record{0, 0};
    for (int n = 0; n < 5; ++n)
        record.insert(record.end(), {1, 1, 0x14, 0x99});
    record.insert(record.end(), body.begin(), body.end());
    return std::make_shared<const EclProgram>(EclProgram::decode(record, "synthetic tour"));
}

void step_to_prompt(RolfTourSession &tour)
{
    for (int n = 0; n < 500 && tour.snapshot().phase == TourPhase::running; ++n)
        tour.advance(0.3);
    check(tour.snapshot().phase != TourPhase::faulted, tour.snapshot().diagnostic.c_str());
    check(tour.snapshot().phase != TourPhase::running, "Host must yield a prompt or finish");
}

void animation_tests()
{
    // Two 8x1 frames: a full first frame, then a delta XORed with that first frame.
    const Bytes record{2,
                       17, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0,
                       0x12, 0x34, 0x56, 0x78,
                       15, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0,
                       0x10, 0, 0, 0};
    const auto animation = opengold::decode_ega_animation(record);
    check(animation && animation.frames.size() == 2, "Decode both animation frames");
    check(animation.frames[0].delay == 17 && animation.frames[1].delay == 15,
          "Keep each frame's original delay");
    check(animation.frames[0].image.rgba[2] == 170 && animation.frames[1].image.rgba[2] == 0,
          "First pixel changes from blue to black in the delta frame");
    check(animation.frames[1].image.rgba[5] == 170 &&
          animation.frames[1].image.rgba[9] == animation.frames[0].image.rgba[9],
          "Pixels outside the delta keep the first frame's colors");
    auto truncated = record;
    truncated.pop_back();
    check(!opengold::decode_ega_animation(truncated), "Truncated animation rejected");
    auto resized = record;
    resized[32] = 2; // Second frame header claims a different width.
    check(!opengold::decode_ega_animation(resized), "Frames of different sizes rejected");
}

void cluebook_portrait_tests()
{
    struct PortraitCase
    {
        unsigned script;
        std::string_view dialogue, portrait;
    };
    for (const auto &target : std::array{
             PortraitCase{0, "THE HARBOR MASTER TELLS YOU BOATS LEAVE", "NPCs/harbor-master.png"},
             PortraitCase{0, "YOU ARE USHERED INTO THE BISHOP'S STUDY. BISHOP BRACCIO SPEAKS", "NPCs/bishop-braccio.png"},
             PortraitCase{0, "YOU ARE USHERED INTO THE STUDY WHERE DIRTEN WAITS", "NPCs/dirten.png"},
             PortraitCase{8, "AT YOUR ENTRY, THE COUNCIL CLERK BEGINS", "NPCs/council-clerk.png"},
             PortraitCase{8, "COUNCILMAN CADORNA CONFRONTS YOU", "NPCs/councilman-cadorna.png"},
             PortraitCase{8, "IN THE COUNCIL CHAMBERS, THE ENTIRE CITY COUNCIL IS WAITING", "NPCs/mayor-ulrich-eberhard.png"},
             PortraitCase{20, "'I MAY HAVE A USE FOR YOU,' THE MAN SAYS", "NPCs/ohlo.png"},
             PortraitCase{20, "SEATED AT A TABLE IS A RAGGED OLD WOMAN", "NPCs/slums-fortune-teller.png"},
             PortraitCase{20, "'WAIT A MOMENT,' THE MAN SAYS", "NPCs/ohlos-potion-keeper.png"},
             PortraitCase{29, "A WIDE-EYED WOMAN IS SEATED ON A RUG", "NPCs/kutos-well-seer.png"},
         })
    {
        TourSnapshot state;
        state.tour_finished = true;
        state.script_id = target.script;
        state.sprite_frame = -1; // Original scene picture, not an approach sprite.
        state.dialogue = target.dialogue;
        check(opengold::speaking_npc_portrait(state) == target.portrait,
              "Cluebook NPC dialogue selects its own portrait");
        state.sprite_frame = 1;
        check(opengold::speaking_npc_portrait(state).empty(),
              "The approach sprite remains until the character reaches the party");
        state.sprite_frame = 0;
        check(opengold::speaking_npc_portrait(state) == target.portrait,
              "The nearest encounter pose can show its NPC portrait");
        ++state.script_id;
        check(opengold::speaking_npc_portrait(state).empty(),
              "A shared line or sprite in another script does not borrow the NPC portrait");
    }
}

void wall_art_tests()
{
    Bytes picture_record(17 + 8 * 4, 0);
    picture_record[0] = 8;
    picture_record[2] = 1;
    picture_record[8] = 1;
    picture_record[17] = 0x8d;
    const auto picture = opengold::decode_ega_picture(picture_record);
    check(picture && picture.image.width == 8 && picture.image.rgba[0] == 85 &&
          picture.image.rgba[3] == 255 && picture.image.rgba[4] == 255 &&
          picture.image.rgba[5] == 85,
          "Picture decoder uses opaque normal EGA colors");
    picture_record.pop_back();
    check(!opengold::decode_ega_picture(picture_record), "Truncated portrait rejected");
    Bytes tiles_record(17 + 4 * 32, 0);
    tiles_record[0] = 8;
    tiles_record[2] = 1;
    tiles_record[8] = 4;
    std::fill_n(tiles_record.begin() + 17, 32, 0x11);
    std::fill_n(tiles_record.begin() + 49, 32, 0x88);
    std::fill_n(tiles_record.begin() + 113, 32, 0xdd);
    auto tiles = decode_wall_tiles(tiles_record);
    check(tiles && tiles->size() == 4 && (*tiles)[1][63] == 8, "Decode normal 8x8 tile frames");
    tiles_record.pop_back();
    check(!decode_wall_tiles(tiles_record), "Reject truncated tile record");
    tiles_record.push_back(0xdd);
    tiles_record[2] = 2;
    check(!decode_wall_tiles(tiles_record), "Reject unsupported tile dimensions");

    Bytes definitions(156 * 2, 1);
    std::fill(definitions.begin() + 156, definitions.end(), 2);
    auto art = decode_wall_art(definitions, *tiles);
    check(art && art->appearances.size() == 2, "Two appearances with ten views each");
    const auto &face = art->appearances[0][6];
    check(face.width == 56 && face.height == 64, "Near-facing art retains authored dimensions");
    check(face.rgba[0] == 85 && face.rgba[3] == 255, "Wall palette index eight is opaque gray");
    check(art->appearances[1][6].rgba[0] == 0 && art->appearances[1][6].rgba[3] == 255,
          "Black is opaque in wall art");
    definitions[54] = 3;
    definitions[55] = 0;
    const auto cutouts = decode_wall_art(definitions, *tiles);
    check(cutouts && cutouts->appearances[0][6].rgba[3] == 0 &&
          cutouts->appearances[0][6].rgba[8 * 4 + 3] == 0,
          "Pink and tile zero reveal the backdrop");
    definitions[55] = 4;
    check(!decode_wall_art(definitions, *tiles), "Out-of-bank tile reference rejected");
    definitions.pop_back();
    check(!decode_wall_art(definitions, *tiles), "Partial appearance rejected");

    GeoMap map;
    map.cells[8 * 16 + 8].walls[0] = 1;
    map.cells[7 * 16 + 8].walls[2] = 2;
    map.cells[7 * 16 + 8].walls[0] = 2;
    auto view = compose_exploration_view(map, *art, 8, 8, MapDirection::north);
    check(view.width == 88 && view.height == 88 && view.rgba[(40 * 88 + 44) * 4] == 85,
          "Near face hides farther opaque artwork");
    check(view.rgba[(8 * 88 + 16) * 4] == 85 && view.rgba[(71 * 88 + 71) * 4] == 85,
          "Full near face fits inside the frame");
    view = compose_exploration_view(map, *art, 8, 7, MapDirection::south);
    check(view.rgba[(40 * 88 + 44) * 4] == 0, "Opposite edge keeps its distinct artwork");
    for (unsigned facing = 0; facing < 4; ++facing)
    {
        GeoMap rotated;
        rotated.cells[8 * 16 + 8].walls[facing] = 1;
        const auto image = compose_exploration_view(rotated, *art, 8, 8, MapDirection{facing});
        check(image.rgba[(40 * 88 + 44) * 4] == 85, "Directional wall ID follows every facing");
    }
    GeoMap empty;
    const auto background = compose_exploration_view(empty, *art, 0, 0, MapDirection::west);
    empty.cells[0].doors[3] = 1;
    check(compose_exploration_view(empty, *art, 0, 0, MapDirection::west).rgba == background.rgba,
          "Door interaction bits do not fabricate door artwork at map boundaries");
}

void fog_visibility_tests()
{
    WallTiles tiles(2);
    tiles[1].fill(8);
    const auto decoded = decode_wall_art(Bytes(156, 1), tiles);
    check(decoded.has_value(), "Authored opaque wall fixture decodes");
    auto art = *decoded;
    GeoMap corridor;
    for (auto &cell : corridor.cells)
        cell.walls.fill(1);
    corridor.cells[8 * 16 + 8].walls[0] = 0;
    corridor.cells[7 * 16 + 8].walls[0] = 0;
    auto view = render_exploration_view(corridor, art, 8, 8, MapDirection::north);
    check(view.visible.count() == 3 && view.visible.test(8 * 16 + 8) &&
          view.visible.test(7 * 16 + 8) && view.visible.test(6 * 16 + 8),
          "The three rendered corridor cells are visible, without rooms behind side walls");
    corridor.cells[7 * 16 + 8].walls[0] = 1;
    check(render_exploration_view(corridor, art, 8, 8, MapDirection::north).visible.count() == 2,
          "Middle wall hides the far corridor cell");
    corridor.cells[8 * 16 + 8].walls[0] = 1;
    view = render_exploration_view(corridor, art, 8, 8, MapDirection::north);
    check(view.visible.count() == 1 && view.visible.test(8 * 16 + 8),
          "A closed room reveals only the occupied cell");
    corridor.cells[8 * 16 + 8].doors[0] = 1;
    check(render_exploration_view(corridor, art, 8, 8, MapDirection::north).visible == view.visible,
          "A traversable but opaque door does not reveal what is behind it");

    // Cut a window in the exact artwork being rendered. Its hole must expose
    // both the floor and distant wall without a separate collision/FOV guess.
    art.appearances.push_back(art.appearances.front());
    auto &front = art.appearances.back()[static_cast<unsigned>(WallView::near_front)];
    for (unsigned y = 32; y < 56; ++y)
        for (unsigned x = 24; x < 32; ++x)
            front.rgba[(y * front.width + x) * 4 + 3] = 0;
    corridor.cells[8 * 16 + 8].walls[0] = 2;
    corridor.cells[7 * 16 + 8].walls[0] = 0;
    check(render_exploration_view(corridor, art, 8, 8, MapDirection::north).visible.count() == 3,
          "A transparent opening reveals the cells actually drawn through it");

    const auto open = render_exploration_view({}, {}, 8, 8, MapDirection::north);
    check(open.visible.test(7 * 16 + 8) && open.visible.test(6 * 16 + 8) &&
          !open.visible.test(5 * 16 + 8) && !open.visible.test(9 * 16 + 8),
          "Open floor reveals the forward view, not distant or rear cells");
    for (unsigned facing = 0; facing < 4; ++facing)
    {
        const auto rotated = render_exploration_view({}, {}, 8, 8, MapDirection{facing});
        std::bitset<256> expected;
        for (unsigned y = 0; y < 16; ++y)
            for (unsigned x = 0; x < 16; ++x)
                if (open.visible.test(y * 16 + x))
                {
                    int dx = int(x) - 8, dy = int(y) - 8;
                    for (unsigned turn = 0; turn < facing; ++turn)
                    {
                        const int old_x = dx;
                        dx = -dy;
                        dy = old_x;
                    }
                    expected.set((8 + dy) * 16 + 8 + dx);
                }
        check(rotated.visible == expected, "Visibility rotates with all four facings");
    }
    for (unsigned x :
            {
                0u, 15u
            })
        for (unsigned y :
                {
                    0u, 15u
                })
            for (unsigned facing = 0; facing < 4; ++facing)
            {
                const auto edge = render_exploration_view({}, {}, x, y, MapDirection{facing});
                check(edge.visible.test(y * 16 + x) && !edge.visible.test((15 - y) * 16 + (15 - x)),
                      "Viewport clipping never wraps visibility to the other map edge");
            }

    const auto start = program({9, 0, 8, 1, 0x4b, 0xc0, 9, 0, 8, 1, 0x4c, 0xc0, 0});
    RolfTourSession tour({}, start, {}, 0x9914);
    (void)tour.observe_view();
    check(tour.snapshot().seen.none(), "An uninitialized pose cannot reveal the default origin");
    tour.advance(0);
    check(tour.snapshot().seen == tour.snapshot().visited && tour.snapshot().visited.count() == 1,
          "Spawning reveals the occupied cell before a view is shown");
    (void)tour.observe_view();
    const auto north = tour.snapshot().seen;
    check(north == open.visible && tour.snapshot().visited.count() == 1,
          "Seeing forward does not count as visiting");
    const auto revision = tour.snapshot().revision;
    (void)tour.observe_view();
    check(tour.snapshot().revision == revision, "Repainting the same view does not mutate history");
    tour.explore(ExplorationCommand::turn_around);
    (void)tour.observe_view();
    check((tour.snapshot().seen & north) == north && tour.snapshot().seen.test(10 * 16 + 8),
          "Turning reveals new cells and remembers the previous view");
    tour.restart();
    tour.advance(0);
    check(tour.snapshot().seen.count() == 1, "A new campaign clears previous map knowledge");
}

void shopping_tests()
{
    // Tour exits at 9914; every normal entry invokes a generated shop event.
    Bytes bytes{0, 0};
    for (int n = 0; n < 5; ++n)
        bytes.insert(bytes.end(), {1, 1, 0x15, 0x99});
    const Bytes body{0, 39, 0, 0, 0,  0, 0, 0, 0, 0,    0,    0,  0,
                     0, 0,  0, 0, 54, 9, 0, 1, 1, 0x6c, 0x6e, 36, 0};
    bytes.insert(bytes.end(), body.begin(), body.end());
    auto p = std::make_shared<const EclProgram>(EclProgram::decode(bytes, "synthetic shop"));
    auto resources = std::make_shared<PhlanResources>();
    resources->programs.emplace(0, p);
    Equipment sword;
    sword.stored.stored_name = "Test sword";
    sword.stored.value = 10;
    Equipment expensive = sword;
    expensive.stored.value = 10000;
    resources->treasure[54] = {sword, expensive};
    RolfTourSession town({}, p, {}, 0x9914, {}, resources);
    town.advance(0);
    check(town.party().wealth[3] == 9999 && town.party().inventory.empty(),
          "One fighter starts with 9999 gold and no inventory");
    check(town.explore(ExplorationCommand::look), "Look schedules original search entry");
    step_to_prompt(town);
    check(town.snapshot().phase == TourPhase::shopping, "Script COMBAT shop flag opens a shop");
    const auto ticket = town.snapshot().continue_ticket;
    check(!town.explore(ExplorationCommand::forward), "Movement is locked while shopping");
    check(!town.buy(ticket + 1, 0) && !town.buy(ticket, 9),
          "Stale shop and invalid stock index rejected");
    check(!town.buy(ticket, 1) && town.party().wealth[3] == 9999,
          "Unaffordable item cannot deduct gold");
    check(town.buy(ticket, 0) && town.party().wealth[3] == 9989 &&
          town.party().inventory.size() == 1,
          "Purchase charges actual price and adds inventory");
    for (int i = 1; i < 16; ++i)
        check(town.buy(ticket, 0), "Separate purchased instances fit inventory");
    check(!town.buy(ticket, 0) && town.party().wealth[3] == 9839, "Full inventory does not charge");
    check(town.leave_shop(ticket) && !town.leave_shop(ticket), "Shop reply completes once");
    step_to_prompt(town);
    check(town.script_variable(0x6E6C) == 0 && town.script_variable(0x6BC1) == 9839,
          "Shop results synchronize script purse and mode");
    check(town.explore(ExplorationCommand::look), "Can revisit shop");
    step_to_prompt(town);
    check(!town.buy(ticket, 0) && town.shop_stock().size() == 4,
          "Old shop ticket stays invalid on revisit");
    // This fixture intentionally omits CLEAR MONSTERS: TREASURE accumulates lists.
    town.restart();
    town.advance(0);
    check(town.party().wealth[3] == 9999 && town.party().inventory.empty(),
          "Replay resets demo economy");

    Bytes failing_bytes{0, 0};
    for (int n = 0; n < 5; ++n)
        failing_bytes.insert(failing_bytes.end(), {1, 1, 0x15, 0x99});
    const Bytes failing_body{0,    9,  0, 5, 1, 0xc1,
                             0x6b, 56, 0, 0, 0}; // Purse write, then unimplemented training.
    failing_bytes.insert(failing_bytes.end(), failing_body.begin(), failing_body.end());
    auto failing_program = std::make_shared<const EclProgram>(
                               EclProgram::decode(failing_bytes, "unsupported town service"));
    RolfTourSession failure({}, failing_program, {}, 0x9914, {}, resources);
    failure.advance(0);
    failure.explore(ExplorationCommand::look);
    step_to_prompt(failure);
    check(failure.snapshot().phase == TourPhase::awaiting_continue &&
          failure.script_diagnostics().size() == 1,
          "Unsupported service produces a recoverable event notice");
    check(failure.party().wealth[3] == 9999 && failure.script_variable(0x6BC1) == 9999,
          "Unsupported event rolls back script and party mutations");
    check(failure.continue_dialogue(failure.snapshot().continue_ticket) &&
          failure.snapshot().phase == TourPhase::completed,
          "Acknowledging unsupported event restores free exploration");
}

std::size_t peaceful_choice(const TourSnapshot &s)
{
    for (const auto *safe :
            {"NO", "LEAVE", "RUN", "GO", "NONE", "EXIT"
            })
        for (std::size_t n = 0; n < s.choices.size(); ++n)
            if (s.choices[n] == safe)
                return n;
    return s.choices.size() - 1;
}

void settle_town(RolfTourSession &town, unsigned shop_x = 16, unsigned shop_y = 16,
                 unsigned visit_x = 16, unsigned visit_y = 16, bool *reached = nullptr)
{
    for (unsigned n = 0; n < 2000; ++n)
    {
        const auto s = town.snapshot();
        if (reached && s.pose.x == visit_x && s.pose.y == visit_y)
            *reached = true;
        if (s.phase == TourPhase::completed)
            return;
        check(s.phase != TourPhase::faulted, s.diagnostic.c_str());
        if (s.phase == TourPhase::awaiting_continue)
        {
            if (s.pose.x == shop_x && s.pose.y == shop_y &&
                    s.dialogue.starts_with("THE SHOP"))
                check(!opengold::phlan_shopkeeper_portrait(s).empty(),
                      "The shopkeeper portrait appears for the greeting");
            town.choose(s.continue_ticket, s.pose.x == shop_x && s.pose.y == shop_y &&
                        s.dialogue.find("SHOP") != std::string::npos
                        ? 0
                        : peaceful_choice(s));
        }
        else if (s.phase == TourPhase::awaiting_input)
            town.input(s.continue_ticket, "0");
        else if (s.phase == TourPhase::shopping)
        {
            if (s.pose.x == shop_x && s.pose.y == shop_y)
                return;
            town.leave_shop(s.continue_ticket);
        }
        else
            town.advance(.3);
    }
    check(false, "Town event exceeded bounded continuation count");
}

bool walk_to(RolfTourSession &town, unsigned tx, unsigned ty, bool shop = false)
{
    constexpr std::array<int, 4> dx{0, 1, 0, -1}, dy{-1, 0, 1, 0};
    std::set<std::pair<int, int>> refused_edges;
    for (unsigned step = 0; step < 400; ++step)
    {
        const auto p = town.snapshot().pose;
        if (p.x == tx && p.y == ty)
            return true;
        std::array<int, 256> previous;
        previous.fill(-1);
        std::queue<int> cells;
        const int origin = p.y * 16 + p.x;
        cells.push(origin);
        previous[origin] = origin;
        while (!cells.empty())
        {
            const int cell = cells.front();
            cells.pop();
            for (unsigned d = 0; d < 4; ++d)
            {
                const int x = cell % 16 + dx[d], y = cell / 16 + dy[d];
                if (x < 0 || y < 0 || x >= 16 || y >= 16)
                    continue;
                const auto &a = town.map().at(cell % 16, cell / 16);
                const auto &b = town.map().at(x, y);
                const unsigned reverse = (d + 2) % 4;
                if (a.doors[d] > 1 || b.doors[reverse] > 1 || (a.walls[d] && !a.doors[d]) ||
                        (b.walls[reverse] && !b.doors[reverse]))
                    continue;
                const int next = y * 16 + x;
                if (previous[next] >= 0 || refused_edges.contains({cell, next}))
                    continue;
                previous[next] = cell;
                cells.push(next);
            }
        }
        int next = ty * 16 + tx;
        if (previous[next] < 0)
            return false;
        while (previous[next] != origin)
            next = previous[next];
        const unsigned facing = next % 16 > int(p.x)   ? 1
                                : next % 16 < int(p.x) ? 3
                                : next / 16 > int(p.y) ? 2
                                : 0;
        const bool forward = index(p.facing) == facing;
        town.explore(forward ? ExplorationCommand::forward : ExplorationCommand::turn_right);
        bool reached = false;
        settle_town(town, shop ? tx : 16, shop ? ty : 16, tx, ty, &reached);
        if (reached)
            return true;
        if (forward && town.snapshot().pose.x * 1 + town.snapshot().pose.y * 16 != unsigned(next))
            refused_edges.insert({origin, next});
    }
    return false;
}

void installed_town(const RolfTourSession &finished)
{
    // Reach every numbered town location through actual movement and doors.
    // Original entry dispatch picks ECL3:0, :8 (City Hall), and :11 (training).
    std::set<unsigned> events, scripts;
    std::set<std::string> limitations;
    unsigned reached = 0;
    for (unsigned y = 0; y < 16; ++y)
        for (unsigned x = 0; x < 16; ++x)
        {
            const auto event = finished.map().at(x, y).event_number();
            if (!event)
                continue;
            auto town = finished.detached_copy();
            if (!walk_to(town, x, y))
            {
                std::cout << "Town route unavailable " << x << ',' << y << " event " << event
                          << '\n';
                for (const auto &d : town.script_diagnostics())
                    limitations.insert(d);
                continue;
            }
            ++reached;
            events.insert(event);
            scripts.insert(town.snapshot().script_id);
            town.explore(ExplorationCommand::look);
            settle_town(town);
            for (const auto &d : town.script_diagnostics())
                limitations.insert(d);
            check(town.party().wealth[3] == 9999,
                  "Exploration without purchases preserves starting gold");
        }
    std::cout << "Town coverage: " << reached << " event cells, " << events.size() << " event IDs, "
              << scripts.size() << " programs.\n";
    for (const auto &d : limitations)
        std::cout << "Town unsupported branch: " << d << '\n';
    check(events.size() >= 30 && scripts == std::set<unsigned> {0, 8, 11},
          "Walkable town includes City Hall and training scripts");
    // The cluebook marks twelve distinct shops, even where stock lists repeat.
    struct ShopCase
    {
        unsigned x, y, stock;
        std::string_view portrait;
    };
    // Every general store adds Torches, Acid and Alchemist's Fire to its seven
    // original items.
    for (const auto target : std::array{
             ShopCase{13, 8, 57, "NPCs/phlan-arms-13-08.png"},
             ShopCase{8, 11, 57, "NPCs/phlan-arms-08-11.png"},
             ShopCase{11, 12, 57, "NPCs/phlan-arms-11-12.png"},
             ShopCase{9, 13, 57, "NPCs/phlan-arms-09-13.png"},
             ShopCase{15, 8, 10, "NPCs/phlan-general-15-08.png"},
             ShopCase{9, 10, 10, "NPCs/phlan-general-09-10.png"},
             ShopCase{12, 10, 10, "NPCs/phlan-general-12-10.png"},
             ShopCase{9, 11, 10, "NPCs/phlan-general-09-11.png"},
             ShopCase{11, 11, 10, "NPCs/phlan-general-11-11.png"},
             ShopCase{11, 10, 13, "NPCs/phlan-silver-11-10.png"},
             ShopCase{10, 13, 13, "NPCs/phlan-silver-10-13.png"},
             ShopCase{8, 10, 11, "NPCs/phlan-jeweler-08-10.png"},
         })
    {
        auto town = finished.detached_copy();
        check(walk_to(town, target.x, target.y, true), "Walk to original shop");
        check(town.snapshot().phase == TourPhase::shopping,
              "Entering shop and answering Yes opens actual stock");
        check(town.shop_stock().size() == target.stock, "Original shop-specific stock count");
        check(opengold::speaking_npc_portrait(town.snapshot()) == target.portrait,
              "Shopping shows this location's unique shopkeeper");
        const auto affordable = std::find_if(town.shop_stock().begin(), town.shop_stock().end(),
                                             [](const auto & item)
        {
            return item.stored.value <= 9999;
        });
        check(affordable != town.shop_stock().end(),
              "Shop has merchandise affordable with starting purse");
        const auto item_index = static_cast<std::size_t>(affordable - town.shop_stock().begin());
        const auto ticket = town.snapshot().continue_ticket;
        const auto price = affordable->stored.value;
        check(town.buy(ticket, item_index), "Buy original item");
        check(town.party().wealth[3] == 9999 - price && town.party().inventory.size() == 1,
              "Actual price and inventory persist");
        check(town.leave_shop(ticket), "Exit original shop");
        check(opengold::phlan_shopkeeper_portrait(town.snapshot()).empty(),
              "Shopkeeper portrait ends when shopping ends");
        settle_town(town);
    }
}

void synthetic()
{
    const auto p =
        program({9,  0,  3,    1,    0x4b, 0xc0, 9,    0,    4,  1,  0x4c, 0xc0, 9, 0,
                 1,  1,  0x4d, 0xc0, 45,   1,    0x90, 0x2c, 12, 0,  12,   0,    2, 0,
                 9,  58, 13,   58,   13,   18,   128,  3,    4,  32, 192, // Generated "ABC" text.
                 43, 1,  1,    0x98, 0,    1,    128,  3,    4,  32, 192,  49,   9, 0,
                 4,  1,  0x4b, 0xc0, 45,   1,    0x90, 0x2c, 0});
    EclMachine vm(p);
    check(!vm.start_at_for_inspection(0x9901), "Reject entry table interior");
    check(vm.state() == EclState::idle, "Invalid inspection start does not mutate state");
    check(vm.start_at_for_inspection(0x9914), "Explicit isolated start");
    check(!vm.start_at_for_inspection(0x9914), "Cannot replace a running invocation");
    GeoMap map;
    map.cells[4 * 16 + 3].event_raw = 7;
    map.cells[4 * 16 + 4].walls[1] = 1;
    map.cells[3 * 16 + 4].doors[0] = 1;
    RolfTourSession tour(map, p, {}, 0x9914);
    check(!tour.explore(ExplorationCommand::forward), "Movement locked while scripts run");
    tour.advance(0);
    check(tour.snapshot().sprite_frame == 2, "Far sprite setup and delay");
    check(tour.script_variable(0xC04F) == 7, "Redraw derives current map event");
    tour.advance(0);
    check(tour.snapshot().sprite_frame == 2, "Delay is nonblocking and not reissued");
    step_to_prompt(tour);
    check(tour.snapshot().pose == PartyPose{3, 4, MapDirection::east}, "Script controls party pose");
    check(tour.snapshot().sprite_frame == 0 && tour.snapshot().dialogue == "ABC",
          "Near sprite and decoded text");
    const auto ticket = tour.snapshot().continue_ticket;
    check(!tour.continue_dialogue(ticket + 1), "Wrong ticket rejected");
    check(tour.continue_dialogue(ticket) && !tour.continue_dialogue(ticket), "Reply applied once");
    step_to_prompt(tour);
    check(tour.snapshot().phase == TourPhase::completed && tour.snapshot().sprite_frame == -1,
          "Tour completes and hides encounter");
    check(tour.snapshot().pose == PartyPose{4, 4, MapDirection::east}, "Later scripted redraw");
    const auto before_turn = tour.snapshot().pose;
    check(tour.explore(ExplorationCommand::turn_around), "Turn around succeeds");
    check(tour.snapshot().pose ==
          PartyPose{before_turn.x, before_turn.y, reversed(before_turn.facing)},
          "Turn around changes facing without moving");
    tour.explore(ExplorationCommand::turn_around);
    check(tour.snapshot().pose == before_turn, "Two half turns restore facing");
    check(!tour.explore(ExplorationCommand::forward), "Wall blocks exploration");
    check(tour.explore(ExplorationCommand::turn_left), "Turn after completion");
    check(tour.script_variable(0xC04D) == 0, "Exploration and VM share authoritative pose");
    check(tour.explore(ExplorationCommand::forward), "Open edge permits a step");
    check(!tour.explore(ExplorationCommand::forward), "Door blocks inspection movement");
    tour.explore(ExplorationCommand::turn_right);
    check(tour.explore(ExplorationCommand::forward), "Eastward open edge permits a step");
    tour.explore(ExplorationCommand::turn_right);
    check(tour.explore(ExplorationCommand::forward), "Southward open edge permits a step");
    tour.explore(ExplorationCommand::turn_right);
    check(!tour.explore(ExplorationCommand::forward), "Neighbor wall blocks westward movement");
    tour.explore(ExplorationCommand::turn_right);
    for (unsigned i = 0; i < 4; ++i)
        check(tour.explore(ExplorationCommand::forward), "Northward steps stay in the map");
    check(!tour.explore(ExplorationCommand::forward), "Boundary prevents wrapping");
    tour.restart();
    step_to_prompt(tour);
    check(!tour.continue_dialogue(ticket), "Stale pre-restart ticket rejected");
    const auto bad = program({45, 1, 0, 0x80, 0});
    RolfTourSession unsupported({}, bad, {}, 0x9914);
    unsupported.advance(0);
    check(unsupported.snapshot().phase == TourPhase::faulted,
          "Unsupported services stop with diagnostics");
}

void installed_monster_picture(const char *directory)
{
    // The Slums orcs' close-up alternates a ready pose and a striking pose; the
    // zero-tick frames between them are never held on screen.
    std::ifstream file(std::filesystem::path(directory) / "PIC2.DAX", std::ios::binary);
    const Bytes bytes{std::istreambuf_iterator<char>(file), {}};
    const auto archive = opengold::decode_dax_archive(bytes);
    check(archive.status == opengold::FormatResult::ok, "Decode PIC2.DAX");
    const auto orc = std::find_if(archive.records.begin(), archive.records.end(),
                                  [](const auto & record)
    {
        return record.id == 4;
    });
    check(orc != archive.records.end(), "PIC2.DAX holds the orc close-up");
    const auto animation = opengold::decode_ega_animation(orc->bytes);
    check(animation && animation.frames.size() == 4, "Orc close-up has four frames");
    std::vector<std::uint32_t> delays;
    for (const auto &frame : animation.frames)
        delays.push_back(frame.delay);
    check(delays == std::vector<std::uint32_t> {15, 0, 7, 0}, "Orc close-up keeps its timing");
    check(animation.frames[0].image.width == 88 && animation.frames[0].image.height == 88,
          "Orc close-up fills the 88x88 view");
    check(animation.frames[0].image.rgba != animation.frames[2].image.rgba,
          "Ready and striking poses differ");
}

void installed(const char *directory)
{
    installed_monster_picture(directory);
    auto tour = RolfTourSession::load(directory);
    check(tour.wall_art().appearances.size() == 15, "Load all fifteen Phlan appearances");
    check(tour.wall_art().appearances[5][6].rgba != tour.wall_art().appearances[7][6].rgba,
          "Distinct stone masonry patterns retained");
    check(tour.map().at(4, 3).walls[2] == 11 && tour.map().at(5, 2).walls[1] == 12 &&
          tour.map().at(11, 2).walls[2] == 13,
          "Original landmark IDs select city hall, training hall and temple");
    unsigned prompts = 0;
    while (true)
    {
        step_to_prompt(tour);
        if (tour.snapshot().phase == TourPhase::completed)
            break;
        const auto &s = tour.snapshot();
        ++prompts;
        std::cout << "Prompt " << prompts << " at " << s.pose.x << ',' << s.pose.y << " facing "
                  << index(s.pose.facing) << '\n';
        // The view shows Rolf's portrait exactly when he is fully approached.
        check((s.sprite_frame == 0) == (prompts == 1),
              "Rolf is fully approached for his welcome only");
        check(prompts <= 16, "Bounded installed tour");
        check(!s.dialogue.empty(), "Original dialogue shown at each pause");
        const auto image = compose_exploration_view(tour.map(), tour.wall_art(), s.pose.x, s.pose.y,
            s.pose.facing);
        check(image.rgba.size() == 88 * 88 * 4, "Every tour pause composes original artwork");
        check(!tour.explore(ExplorationCommand::forward), "No movement during original dialogue");
        check(tour.continue_dialogue(s.continue_ticket), "Original Continue accepted");
    }
    check(prompts == 8, "Welcome, six landmarks, farewell");
    check(tour.snapshot().redraws > 20 && tour.snapshot().footsteps > 20,
          "Tour extends across original table-driven route");
    check(tour.script_variable(0x4AC5) == 1, "Original tour flag retained");
    std::cout << "Installed tour complete: " << prompts << " prompts, " << tour.snapshot().redraws
              << " redraws.\n";
    installed_town(tour);
}
} // namespace

int main()
{
    try
    {
        wall_art_tests();
        animation_tests();
        cluebook_portrait_tests();
        fog_visibility_tests();
        shopping_tests();
        synthetic();
        if (const auto directory = std::getenv("OPENGOLD_GAME_DIR"))
            installed(directory);
        std::cout << "Tour tests passed.\n";
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
