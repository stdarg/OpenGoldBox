// The Buck Rogers games' solar map on the exploration screen
// (docs/games/countdown.md, "The solar map"): the map takes the 3D view's
// place; the side column shows the body's picture, the rocket's cell and
// heading, the ship's fuel, the game clock and the map's menu with an
// eight-way pad. The session flies the rocket and the scripts answer at a
// body; this file only draws and forwards input.
#include "rolf_tour_view.h"
#include "application_settings.h"
#include "godot_images.h"
#include "godot_nodes.h"
#include "guarded_handlers.h"
#include "localization.h"

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/rich_text_label.hpp>
#include <godot_cpp/classes/viewport.hpp>

#include <array>

using namespace godot;
using opengold::por::ExplorationCommand;
using opengold::por::TourPhase;
using opengold::por::TourSnapshot;
using presentation::required_node;
namespace og = opengold::original;

namespace
{
// The bodies' names in the scripts' order (planet n + 1 at body n): the four
// planets as the original names them under their pictures, then each
// asteroid by its own arrival menu (ECL2:34 8499h to 88B2h).
constexpr std::array<const char *, og::solar_bodies> body_names{
    N_("Mercury"), N_("Venus"),   N_("Earth"),  N_("Mars"),   N_("Ceres"),
    N_("Vesta"),   N_("Fortuna"), N_("Pallas"), N_("Psyche"), N_("Juno"),
    N_("Hygeia"),  N_("Aurora"),  N_("Thule")};

// The rocket's headings, in og::RocketHeading's order.
constexpr std::array<const char *, 8> heading_names{
    N_("North"), N_("Northwest"), N_("West"), N_("Southwest"),
    N_("South"), N_("Southeast"), N_("East"), N_("Northeast")};

// The color the original frames the body's picture in (portrait.pas
// DisplayImageBuf), an EGA color: magenta between bodies, then Mercury's,
// Venus's, Earth's and Mars's, and the asteroids' gray.
[[nodiscard]] int frame_color(std::optional<std::size_t> body)
{
    constexpr std::array<int, 4> planets{12, 2, 1, 4};
    constexpr int en_route = 5, asteroids = 8;
    if (!body)
    {
        return en_route;
    }
    return *body < planets.size() ? planets[*body] : asteroids;
}

// The pad's buttons and the step each flies, north and on clockwise.
constexpr std::array<std::pair<const char *, ExplorationCommand>, 8> pad{{
    {"%PadNorth", ExplorationCommand::north},
    {"%PadNortheast", ExplorationCommand::northeast},
    {"%PadEast", ExplorationCommand::east},
    {"%PadSoutheast", ExplorationCommand::southeast},
    {"%PadSouth", ExplorationCommand::south},
    {"%PadSouthwest", ExplorationCommand::southwest},
    {"%PadWest", ExplorationCommand::west},
    {"%PadNorthwest", ExplorationCommand::northwest},
}};

// The step toward a cell from the rocket's: the eight ways by the larger
// of the two distances, as the original reads a click (cbtmap.pas
// DirectionBetween).
[[nodiscard]] std::optional<ExplorationCommand> step_toward(og::OrbitCell from, og::OrbitCell to)
{
    const int dx = static_cast<int>(to.x) - static_cast<int>(from.x);
    const int dy = static_cast<int>(to.y) - static_cast<int>(from.y);
    if (dx == 0 && dy == 0)
    {
        return std::nullopt;
    }
    const auto sign = [](int value)
    {
        return (value > 0) - (value < 0);
    };
    // Mostly along one axis: straight; otherwise diagonal.
    const int sx = 2 * std::abs(dx) < std::abs(dy) ? 0 : sign(dx);
    const int sy = 2 * std::abs(dy) < std::abs(dx) ? 0 : sign(dy);
    for (const auto &[button, command] : pad)
    {
        constexpr std::array<int, 8> columns{0, 1, 1, 1, 0, -1, -1, -1},
            rows{-1, -1, 0, 1, 1, 1, 0, -1};
        const auto way =
            static_cast<std::size_t>(command) - static_cast<std::size_t>(ExplorationCommand::north);
        if (columns[way] == sx && rows[way] == sy)
        {
            return command;
        }
    }
    return std::nullopt;
}

[[nodiscard]] Vector2 cell_point(og::OrbitCell cell)
{
    return {static_cast<float>(cell.x), static_cast<float>(cell.y)};
}
} // namespace

bool RolfTourView::on_solar_map() const
{
    return dax() && dax()->snapshot().solar_map;
}

void RolfTourView::connect_solar_map()
{
    for (const auto &[button, command] : pad)
    {
        required_node<Button>(*this, button)
            .connect("pressed", presentation::guarded(this, &RolfTourView::fly_pad)
                                    .bind(static_cast<std::int64_t>(command)));
    }
    required_node<Button>(*this, "%RocketMove")
        .connect("pressed", presentation::guarded(this, &RolfTourView::toggle_rocket_move));
    required_node<Button>(*this, "%RocketView")
        .connect("pressed", presentation::guarded(this, &RolfTourView::view_rocket));
    required_node<Button>(*this, "%RocketChange")
        .connect("pressed", presentation::guarded(this, &RolfTourView::change_settings));
}

bool RolfTourView::flies_at_once() const
{
    return settings::direct_flight(dax_session().game());
}

void RolfTourView::fly_rocket(ExplorationCommand command)
{
    if (!on_solar_map() || (!flies_at_once() && !rocket_move_mode_))
    {
        return;
    }
    rocket_shown_ = false;
    movement(command);
}

void RolfTourView::fly_pad(std::int64_t command)
{
    fly_rocket(static_cast<ExplorationCommand>(command));
}

void RolfTourView::toggle_rocket_move()
{
    if (!on_solar_map() || flies_at_once())
    {
        return;
    }
    rocket_move_mode_ = !rocket_move_mode_;
    rocket_shown_ = false;
    refresh();
}

void RolfTourView::view_rocket()
{
    if (!on_solar_map() || dax_session().snapshot().phase != TourPhase::completed)
    {
        return;
    }
    rocket_shown_ = !rocket_shown_;
    refresh();
}

void RolfTourView::change_settings()
{
    if (on_solar_map())
    {
        emit_signal("options_requested");
    }
}

bool RolfTourView::solar_map_key(Key key)
{
    switch (key)
    {
    case Key::KEY_UP:
    case Key::KEY_KP_8:
        fly_rocket(ExplorationCommand::north);
        return true;
    case Key::KEY_DOWN:
    case Key::KEY_KP_2:
        fly_rocket(ExplorationCommand::south);
        return true;
    case Key::KEY_LEFT:
    case Key::KEY_KP_4:
        fly_rocket(ExplorationCommand::west);
        return true;
    case Key::KEY_RIGHT:
    case Key::KEY_KP_6:
        fly_rocket(ExplorationCommand::east);
        return true;
    case Key::KEY_HOME:
    case Key::KEY_KP_7:
        fly_rocket(ExplorationCommand::northwest);
        return true;
    case Key::KEY_PAGEUP:
    case Key::KEY_KP_9:
        fly_rocket(ExplorationCommand::northeast);
        return true;
    case Key::KEY_END:
    case Key::KEY_KP_1:
        fly_rocket(ExplorationCommand::southwest);
        return true;
    case Key::KEY_PAGEDOWN:
    case Key::KEY_KP_3:
        fly_rocket(ExplorationCommand::southeast);
        return true;
    case Key::KEY_M:
        toggle_rocket_move();
        return true;
    case Key::KEY_R:
        view_rocket();
        return true;
    case Key::KEY_C:
        change_settings();
        return true;
    // The 3D view's Look, Search and camp have no place on the solar map.
    case Key::KEY_L:
    case Key::KEY_S:
        return true;
    default:
        return false;
    }
}

std::optional<og::OrbitCell> RolfTourView::solar_cell_at(Vector2 point) const
{
    // The map is drawn square and centered in the view's frame (tour_visuals.gd).
    const auto &bounds = required_node<Control>(*this, "%SceneBounds");
    const auto frame = bounds.get_global_rect();
    const float side = std::min(frame.size.x, frame.size.y);
    if (side <= 0.0F)
    {
        return std::nullopt;
    }
    const auto origin = frame.position + (frame.size - Vector2(side, side)) * 0.5F;
    const auto at = (point - origin) / side * static_cast<float>(og::solar_map_cells);
    if (at.x < 0.0F || at.y < 0.0F || at.x >= og::solar_map_cells || at.y >= og::solar_map_cells)
    {
        return std::nullopt;
    }
    return og::OrbitCell{static_cast<unsigned>(at.x), static_cast<unsigned>(at.y)};
}

void RolfTourView::solar_map_pointer(const Ref<InputEvent> &event)
{
    const Ref<InputEventMouseMotion> motion = event;
    if (motion.is_valid())
    {
        const auto cell = solar_cell_at(motion->get_global_position());
        const auto body =
            cell ? og::body_at(dax_session().solar_map_state().phase, *cell) : std::nullopt;
        if (body != hovered_body_)
        {
            hovered_body_ = body;
            required_node<Control>(*this, "TourVisuals").queue_redraw();
        }
        return;
    }
    // A click on the map flies a step toward it, as the original's does.
    const Ref<InputEventMouseButton> click = event;
    if (click.is_null() || !click->is_pressed() || click->get_button_index() != MOUSE_BUTTON_LEFT ||
        dax_session().snapshot().phase != TourPhase::completed)
    {
        return;
    }
    const auto cell = solar_cell_at(click->get_global_position());
    if (!cell)
    {
        return;
    }
    if (const auto command = step_toward(dax_session().solar_map_state().rocket, *cell))
    {
        fly_rocket(*command);
        get_viewport()->set_input_as_handled();
    }
}

void RolfTourView::show_solar_map(const TourSnapshot &s)
{
    const bool solar = on_solar_map();
    for (const char *name : {"%SolarFuel", "%SolarTime", "%SolarFlight"})
    {
        required_node<Control>(*this, name).set_visible(solar);
    }
    for (const char *name :
         {"%Party", "Layout/Page/Body/Side/Commands", "Layout/Page/Body/Side/Turns"})
    {
        required_node<Control>(*this, name).set_visible(!solar);
    }
    // The map is square; the 3D view five wide to six tall.
    constexpr double map_aspect = 1.0, view_aspect = 0.833333;
    auto &scene = required_node<Control>(*this, "%SceneBounds");
    if (static_cast<double>(scene.get_meta("aspect")) != (solar ? map_aspect : view_aspect))
    {
        scene.set_meta("aspect", solar ? map_aspect : view_aspect);
        required_node<Control>(*this, "Layout/Page/Body/Main/Top").call("queue_sort");
    }
    if (!solar)
    {
        solar_course_.clear();
        rocket_move_mode_ = rocket_shown_ = false;
        hovered_body_.reset();
        required_node<Label>(*this, "%Footer")
            .set_text(
                i18n::text("Arrow keys: move / turn   •   L: look   •   S: search   •   M: cast"));
        return;
    }
    const auto &session = dax_session();
    const auto state = session.solar_map_state();
    if (solar_course_.empty() || solar_course_.back() != state.rocket)
    {
        solar_course_.push_back(state.rocket);
    }
    const auto body = session.orbited_body();
    const auto place = body ? i18n::text(body_names.at(*body)) : i18n::text(N_("En route"));
    required_node<Label>(*this, "%Title").set_text(i18n::text(N_("OPENGOLDBOX  /  Solar map")));
    required_node<Label>(*this, "%Location")
        .set_text(i18n::format(N_("Solar system  /  {place}"), {{"place", place}}));
    required_node<Label>(*this, "%MapTitle").set_text(place);
    required_node<Label>(*this, "%Coordinates")
        .set_text(i18n::format(
            N_("Rocket ({x}, {y})   {heading}"),
            {{"x", state.rocket.x},
             {"y", state.rocket.y},
             {"heading", i18n::text(heading_names.at(static_cast<std::size_t>(state.heading)))}}));
    const auto ship = session.team_ship();
    using Ship = opengold::por::RolfTourSession;
    const auto fuel = ship.system(Ship::ShipSystemKind::fuel);
    required_node<Control>(*this, "%FuelGauge")
        .call("set_fuel", static_cast<int>(fuel.now), static_cast<int>(fuel.whole));
    required_node<Label>(*this, "%FuelText")
        .set_text(i18n::format(N_("{left} / {full}"), {{"left", fuel.now}, {"full", fuel.whole}}));
    const auto clock = session.game_clock();
    required_node<Label>(*this, "%SolarTime")
        .set_text(i18n::format(N_("Day {day}, month {month}, year {year}   {hour}:{minute}"),
                               {{"day", clock.day},
                                {"month", clock.month},
                                {"year", clock.year},
                                {"hour", String::num_int64(clock.hour).pad_zeros(2)},
                                {"minute", String::num_int64(clock.minute).pad_zeros(2)}}));
    const bool completed = s.phase == TourPhase::completed;
    const bool at_once = flies_at_once();
    if (at_once)
    {
        rocket_move_mode_ = false;
    }
    const bool flying = completed && (at_once || rocket_move_mode_);
    for (const auto &[button, command] : pad)
    {
        required_node<Button>(*this, button).set_disabled(!flying);
    }
    auto &move = required_node<Button>(*this, "%RocketMove");
    move.set_visible(!at_once);
    move.set_disabled(!completed);
    move.set_text(i18n::text(rocket_move_mode_ ? N_("Done [M]") : N_("Move [M]")));
    required_node<Button>(*this, "%RocketView").set_disabled(!completed);
    required_node<Button>(*this, "%RocketChange").set_disabled(!completed);
    required_node<Label>(*this, "%Movement")
        .set_text(i18n::text(!completed ? N_("Flight paused")
                             : flying   ? N_("Fly  /  arrow keys, numpad for diagonals")
                                        : N_("Move to fly the rocket")));
    required_node<Label>(*this, "%Footer")
        .set_text(i18n::text(
            at_once
                ? N_("Arrows and numpad: fly   •   R: rocket   •   C: change   •   Escape: menu")
                : N_("M: move   •   arrows and numpad: fly   •   R: rocket   •   C: change   •   Escape: menu")));
    // At a body its name heads the dialogue; between bodies the ship's.
    required_node<Label>(*this, "%Speaker")
        .set_text(body ? place : String::utf8(ship.name.c_str()));
    if (!completed)
    {
        rocket_shown_ = false;
        return;
    }
    // Between bodies the script says nothing more.
    auto &dialogue = required_node<RichTextLabel>(*this, "%Dialogue");
    if (rocket_shown_)
    {
        constexpr std::array<const char *, 6> system_names{N_("Hull"),     N_("Sensors"),
                                                           N_("Controls"), N_("Life support"),
                                                           N_("Fuel"),     N_("Engines")};
        // Three systems a line, under the ship's name, which heads the box.
        String text;
        for (std::size_t n = 0; n < ship.systems.size(); ++n)
        {
            text += (n == 0       ? ""
                     : n % 3 == 0 ? "\n"
                                  : "      ") +
                    i18n::format(N_("{system}: {now} of {whole}"),
                                 {{"system", i18n::text(system_names[n])},
                                  {"now", ship.systems[n].now},
                                  {"whole", ship.systems[n].whole}});
        }
        dialogue.set_text(text);
    }
    else if (s.dialogue.empty())
    {
        dialogue.set_text(i18n::text(N_("En route.")));
    }
}

void RolfTourView::solar_visuals(Dictionary &data)
{
    const auto &session = dax_session();
    const auto state = session.solar_map_state();
    const auto bodies = og::place_bodies(state.phase);
    const auto taken = [&](og::OrbitCell cell)
    {
        return cell == og::sun_cell || cell == state.rocket ||
               std::ranges::any_of(bodies,
                                   [cell](const og::PlacedBody &body)
                                   {
                                       return body.cell == cell;
                                   });
    };
    Dictionary solar;
    // The five orbits as faint dots, an OpenGoldBox addition (DEVIATIONS.md).
    PackedVector2Array orbits;
    for (std::size_t orbit = 0; orbit < og::solar_orbits; ++orbit)
    {
        for (const auto cell : og::orbit_cells(orbit))
        {
            if (!taken(cell))
            {
                orbits.push_back(cell_point(cell));
            }
        }
    }
    solar["orbits"] = orbits;
    // The course flown, when the game's option asks for it.
    PackedVector2Array course;
    if (settings::course_flown(session.game()))
    {
        for (const auto cell : solar_course_)
        {
            if (!taken(cell))
            {
                course.push_back(cell_point(cell));
            }
        }
    }
    solar["course"] = course;
    solar["cells"] = static_cast<int>(og::solar_map_cells);
    if (hovered_body_)
    {
        solar["hover_name"] = i18n::text(body_names.at(*hovered_body_));
        solar["hover_cell"] = cell_point(bodies.at(*hovered_body_).cell);
    }
    // The body's picture in the side column, framed in its color with its
    // name under it; between bodies the script's picture, or the rocket's own
    // tile when it shows none.
    const auto body = session.orbited_body();
    if (data.has("picture"))
    {
        solar["picture"] = data["picture"];
    }
    else if (const auto &picture = session.picture())
    {
        if (solar_picture_revision_ != session.snapshot().picture_revision ||
            solar_picture_.is_null())
        {
            solar_picture_ = presentation::image_texture(*picture);
            solar_picture_revision_ = session.snapshot().picture_revision;
        }
        solar["picture"] = solar_picture_;
    }
    else if (wall_image_.is_valid())
    {
        constexpr int cell_pixels = 8;
        const auto tile = wall_image_->get_region(
            Rect2i(static_cast<int>(state.rocket.x) * cell_pixels,
                   static_cast<int>(state.rocket.y) * cell_pixels, cell_pixels, cell_pixels));
        solar["picture"] = ImageTexture::create_from_image(tile);
    }
    solar["frame_color"] = frame_color(body);
    solar["picture_name"] = body ? i18n::text(body_names.at(*body)).to_upper() : String();
    data.erase("picture");
    data.erase("sprite");
    data["solar"] = solar;
}
