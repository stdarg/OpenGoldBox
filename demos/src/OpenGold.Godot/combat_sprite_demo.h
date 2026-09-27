#ifndef OPENGOLDBOX_COMBAT_SPRITE_DEMO_H
#define OPENGOLDBOX_COMBAT_SPRITE_DEMO_H
#include "opengold/character_art.h"
#include "opengold/dungeon_battlefield.h"
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <optional>

// An isolated art review scene; owns no campaign or combat rules session.
class CombatSpriteDemo : public godot::Control
{
    GDCLASS(CombatSpriteDemo, godot::Control)
  public:
    void _ready() override;
    void _process(double delta) override;
    void _draw() override;
    void _input(const godot::Ref<godot::InputEvent> &event) override;
    void request_capture();

  protected:
    static void _bind_methods();
    void _notification(int what);

  private:
    enum class Sizing
    {
        original,
        stretched,
        proportional
    };

    struct Figure
    {
        const char *node;
        const char *label;
        godot::Vector2 cell;
        godot::Vector2 footprint{1, 1};
        Sizing sizing{Sizing::original};
        std::array<godot::Ref<godot::ImageTexture>, 2> poses;
        std::array<godot::Rect2, 2> visible_bounds;
    };

    std::optional<opengold::por::CharacterArt> art_;
    opengold::por::CharacterAppearance appearance_;
    opengold::por::DungeonBattlefield battlefield_;
    std::vector<godot::Ref<godot::ImageTexture>> terrain_;
    std::vector<Figure> figures_;
    unsigned color_bank_{}, color_part_{};
    int zoom_{250};
    double elapsed_{};
    bool action_{}, ready_{}, loaded_{}, center_pending_{};
    bool capture_pending_{};
    godot::Vector2 center_cell_{26, 14};
    void create_controls();
    void load_art();
    void refresh_players();
    void refresh_colors();
    void refresh_figures();
    void layout();
    void draw_map();
    void zoom_by(int amount);
    void change_part(int part, int direction);
    void select_color(int bank, int part);
    void recolor(int index);
    void capture_frame();
};
#endif
