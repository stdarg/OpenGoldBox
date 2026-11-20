#ifndef OPENGOLDBOX_SHIP_COMBAT_VIEW_H
#define OPENGOLDBOX_SHIP_COMBAT_VIEW_H

#include "opengold/buck_rogers_ship_fight.h"
#include "opengold/gold_box_game.h"
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace opengold::por
{
class RolfTourSession;
}

namespace godot
{
class Button;
class Label;
} // namespace godot

// The Buck Rogers games' ship combat screen (docs/games/countdown.md, "Ship
// combat"; scenes/ship_combat.tscn): the enemy ship's picture with its
// maneuver and the range, the range and how far the team's weapons reach,
// both ships' six systems and their weapons, the acting member's orders in
// the original's order, the fight's log and the crew. It draws the fight and
// forwards the player's orders; the rules (buck_rogers_ship_combat.h) play
// it. Countdown to Doomsday and Matrix Cubed share it.
class ShipCombatView : public godot::Control
{
    GDCLASS(ShipCombatView, godot::Control)
  public:
    // A failed handler goes into the fight's log.
    void report_failure(const std::exception &failure) noexcept;

    void _ready() override;
    void _input(const godot::Ref<godot::InputEvent> &event) override;

    // Plays the ship fight the campaign's script waits on, with the player's
    // ship-combat options; at its end the session takes it.
    void play(opengold::por::RolfTourSession &session);

    // The fight is over, its end given, and the player has closed the screen.
    [[nodiscard]] bool done() const noexcept
    {
        return done_;
    }

    // The screen's check: a fight of the team in the campaign save at
    // `save_path` against an enemy of `enemy_type`, as SHIP COMBAT with these
    // operands starts it, on the dice `seed`, from Countdown's files in
    // `folder`, the picture shrinking with the range or not. Its end is
    // followed as the session follows it.
    bool stage_check(const godot::String &folder, const godot::String &save_path,
                     std::int64_t enemy_type, std::int64_t pilot_skill, std::int64_t to_hit,
                     std::int64_t seed, bool picture_shrinks);
    // The check's fight's outcome (4BBAh), -1 before it ends, and whether the
    // team boarded.
    [[nodiscard]] std::int64_t check_outcome() const;
    [[nodiscard]] bool check_boarded() const;
    // The enemy ship's picture's width over the scene's, for the shrink check.
    [[nodiscard]] double picture_share() const;

  protected:
    static void _bind_methods();

  private:
    // What the screen asks for besides the orders.
    enum class Choice : std::uint8_t
    {
        none,
        target,
        load,
        jury_rig,
        ram,
        view
    };

    struct Fight
    {
        opengold::GoldBoxGame game{};
        // SHIP COMBAT's enemy ship, 1 to 5, whose picture the scene draws.
        unsigned enemy_type{1};
        opengold::original::ShipCombat *combat{};
        const opengold::por::ShipFightFiles *files{};
        // Each member's icon, in party order; null where it has none.
        std::vector<godot::Ref<godot::Texture2D>> icons;
        // Gives the fight's end to whoever plays it and returns what followed.
        std::function<const opengold::por::ShipFightAftermath *()> finish;
    };

    // The check's own fight, which no session holds.
    struct CheckFight
    {
        opengold::por::ShipFightFiles files;
        std::shared_ptr<opengold::RandomState> dice;
        opengold::original::ShipCombatOptions options;
        std::optional<opengold::original::ShipCombat> combat;
        std::optional<opengold::por::ShipFightAftermath> aftermath;
    };

    void prepare();
    void begin(Fight fight, bool picture_shrinks);
    void refresh();
    void show_scene();
    void place_picture();
    void show_range_track();
    void show_ship(bool enemy);
    void show_crew();
    void show_orders();
    void show_choice();
    void show_status();
    void take_events();
    void finish_fight();

    void order_pressed(std::int64_t action);
    void choice_pressed(std::int64_t index);
    void submit(const opengold::original::ShipOrder &order);
    void log_line(const godot::String &text, const godot::Color &color);
    [[nodiscard]] godot::String event_text(const opengold::original::ShipEvent &event,
                                           godot::Color &color) const;
    [[nodiscard]] godot::String side_name(opengold::original::ShipSide side) const;
    [[nodiscard]] godot::String member_name(std::size_t member) const;
    [[nodiscard]] bool acting_pilot() const;
    [[nodiscard]] bool enemy_figures_shown() const;

    std::optional<Fight> fight_;
    std::unique_ptr<CheckFight> check_;
    const opengold::por::ShipFightAftermath *aftermath_{};
    Choice choice_{Choice::none};
    // The choices the open question offers, by its buttons.
    std::vector<opengold::original::ShipOrder> offered_;
    bool prepared_{};
    bool picture_shrinks_{true};
    // Someone read the enemy on the sensors (charhlp.pas enemyDetailsShown).
    bool sensed_{};
    // The enemy's last move took it away (shipcbt.pas isEnemyRetreating).
    bool enemy_retreating_{};
    bool finished_{};
    bool done_{};
};

#endif
