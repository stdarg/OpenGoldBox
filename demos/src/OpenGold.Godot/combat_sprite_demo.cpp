#include "combat_sprite_demo.h"
#include "character_colors.h"
#include "../../../src/OpenGoldBox/godot_images.h"
#include "../../../src/OpenGoldBox/godot_nodes.h"
#include "../../../src/OpenGoldBox/godot_path.h"
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/rich_text_label.hpp>
#include <godot_cpp/classes/scroll_container.hpp>
#include <godot_cpp/classes/texture_rect.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/viewport_texture.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>

using namespace godot;

namespace
{
constexpr unsigned player_count = 4;
constexpr int min_zoom = 10, max_zoom = 1000;

String gs(std::string_view value)
{
    return String::utf8(value.data(), value.size());
}

String dimensions(Vector2 size)
{
    return String::num(size.x, 1) + gs(" × ") + String::num(size.y, 1);
}

void color_button(Button &button, unsigned index, bool selected)
{
    button.call("configure", presentation::character_color(index), selected);
}

// Original archives remain local. RAII owns both the file and decoded buffers.
std::vector<std::uint8_t> archive(const std::filesystem::path &directory, std::string_view wanted)
{
    std::optional<std::filesystem::path> path;
    for (const auto &entry : std::filesystem::directory_iterator(directory))
        if (entry.is_regular_file())
        {
            auto name = entry.path().filename().string();
            for (auto &c : name)
                if (c >= 'a' && c <= 'z')
                    c -= 32;
            if (name == wanted)
            {
                if (path)
                    throw std::runtime_error("Ambiguous art archive: " + std::string(wanted));
                path = entry.path();
            }
        }
    if (!path)
        throw std::runtime_error("Missing art archive: " + std::string(wanted));
    const auto size = std::filesystem::file_size(*path);
    if (size > 32 * 1024 * 1024)
        throw std::runtime_error("Art archive exceeds size limit");
    std::ifstream input(*path, std::ios::binary);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    if (!input ||
            !input.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(size)) ||
            input.peek() != EOF)
        throw std::runtime_error("Cannot read art archive: " + std::string(wanted));
    return bytes;
}

Ref<ImageTexture> icon(const std::vector<std::uint8_t> &bytes, unsigned record, unsigned frame = 0)
{
    auto decoded = opengold::decode_ega_combat_icon(bytes, record, frame);
    if (!decoded)
        throw std::runtime_error("Missing or invalid combat sprite pose: " +
                                 std::to_string(record));
    return presentation::image_texture(decoded.image);
}
} // namespace

double CombatSpriteDemo::tile_pixels() const
{
    return get_theme_constant("combat_sprite_tile_pixels", "OpenGoldMetrics");
}

double CombatSpriteDemo::goliath_height() const
{
    return get_theme_constant("combat_sprite_goliath_height_percent", "OpenGoldMetrics") / 100.0;
}

Color CombatSpriteDemo::figure_color(unsigned index) const
{
    return get_theme_color(index < 2 ? "combat_party" : index == 2 ? "score_active_border"
                           : index == 3 ? "combat_sprite_goliath" : "combat_enemy",
                           "OpenGoldPalette");
}

void CombatSpriteDemo::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("request_capture"), &CombatSpriteDemo::request_capture);
    ADD_SIGNAL(MethodInfo("capture_completed", PropertyInfo(Variant::STRING, "path"),
                          PropertyInfo(Variant::STRING, "error")));
}

void CombatSpriteDemo::_ready()
{
    set_texture_filter(TEXTURE_FILTER_NEAREST);
    get_window()->set_min_size(Vector2i(
        get_theme_constant("combat_sprite_min_width", "OpenGoldMetrics"),
        get_theme_constant("combat_sprite_min_height", "OpenGoldMetrics")));
    create_controls();
    if (Engine::get_singleton()->is_editor_hint())
        return;
    try
    {
        load_art();
        loaded_ = true;
        refresh_players();
    }
    catch (const std::exception &error)
    {
        loaded_ = false;
        set_process(false);
        get_node<Label>("Status")->set_text(gs("Cannot load sprite demo: ") + gs(error.what()));
        for (int i = 0; i < get_child_count(); ++i)
            if (auto *button = Object::cast_to<Button>(get_child(i)))
                button->set_disabled(true);
    }
}

void CombatSpriteDemo::create_controls()
{
    const auto connect_button = [this](const char *name, const Callable &pressed)
    {
        get_node<Button>(name)->connect("pressed", pressed);
    };
    connect_button("Minus100", callable_mp(this, &CombatSpriteDemo::zoom_by).bind(-100));
    connect_button("Minus10", callable_mp(this, &CombatSpriteDemo::zoom_by).bind(-10));
    connect_button("Plus10", callable_mp(this, &CombatSpriteDemo::zoom_by).bind(10));
    connect_button("Plus100", callable_mp(this, &CombatSpriteDemo::zoom_by).bind(100));
    connect_button("HeadPrevious", callable_mp(this, &CombatSpriteDemo::change_part).bind(0, -1));
    connect_button("HeadNext", callable_mp(this, &CombatSpriteDemo::change_part).bind(0, 1));
    connect_button("BodyPrevious", callable_mp(this, &CombatSpriteDemo::change_part).bind(1, -1));
    connect_button("BodyNext", callable_mp(this, &CombatSpriteDemo::change_part).bind(1, 1));
    for (unsigned part = 0; part < 6; ++part)
        for (unsigned bank = 0; bank < 2; ++bank)
            connect_button(("Color" + std::to_string(bank) + "_" + std::to_string(part)).c_str(),
                           callable_mp(this, &CombatSpriteDemo::select_color).bind(bank, part));
    for (unsigned index = 0; index < 16; ++index)
    {
        auto *button = get_node<Button>(gs("Palette" + std::to_string(index)));
        button->connect("pressed", callable_mp(this, &CombatSpriteDemo::recolor).bind(index));
        button->set_tooltip_text(gs(presentation::character_colors[index]));
        color_button(*button, index, false);
    }
    get_node<Control>("BattlefieldScroll/Canvas")
        ->connect("draw", callable_mp(this, &CombatSpriteDemo::draw_map));
}

void CombatSpriteDemo::load_art()
{
    auto configured = OS::get_singleton()->get_environment("OPENGOLD_GAME_DIR");
    if (configured.is_empty())
        configured = ProjectSettings::get_singleton()->get_setting("opengold/game_directory", "");
    const auto directory = presentation::path_from_godot(configured);
    art_ = opengold::por::CharacterArt::load(directory);
    appearance_.combat_body = 4; // A weapon and shield expose all customization regions.
    const auto tiles = archive(directory, "DUNGCOM.DAX");
    for (unsigned frame = 0; frame < 25; ++frame)
        terrain_.push_back(icon(tiles, 1, frame));
    // A wide authored room goes through the same dungeon geometry generator as combat.
    opengold::por::GeoMap map;
    for (unsigned x = 7; x <= 10; ++x)
    {
        map.cells[7 * 16 + x].walls[0] = 1;
        map.cells[10 * 16 + x].walls[2] = 1;
    }
    for (unsigned y = 7; y <= 10; ++y)
    {
        map.cells[y * 16 + 7].walls[3] = 1;
        map.cells[y * 16 + 10].walls[1] = 1;
    }
    battlefield_ = opengold::por::dungeon_battlefield(map, 8, 8);
    figures_ = {{"SmallPlayer", "Short player", {23, 13}},
        {"NormalPlayer", "Human", {25, 13}},
        {"GoliathStretched", "1 — Goliath, stretched", {28, 12}, {1, 2}, Sizing::stretched},
        {
            "GoliathProportional",
            "2 — Goliath, proportional",
            {31, 12},
            {1, 2},
            Sizing::proportional
        }
    };
    // Deliberate art-review selections, not gameplay monster-to-art bindings.
    const auto monsters = archive(directory, "CPIC2.DAX");
    constexpr std::array<unsigned, 5> records{0, 2, 4, 26, 31};
    const std::array<Vector2, 5> cells{{{23, 10}, {25, 10}, {28, 10}, {23, 16}, {28, 16}}};
    constexpr std::array<const char *, 5> names{"Kobold", "Goblin", "Orc", "Basilisk", "Troll"};
    constexpr std::array<const char *, 5> nodes{"Monster0", "Monster1", "Monster2", "Monster3",
            "Monster4"};
    for (unsigned i = 0; i < records.size(); ++i)
    {
        Figure figure{nodes[i], names[i], cells[i]};
        for (unsigned pose = 0; pose < 2; ++pose)
        {
            figure.poses[pose] = icon(monsters, records[i] + pose * 128);
            figure.visible_bounds[pose] = figure.poses[pose]->get_image()->get_used_rect();
        }
        figures_.push_back(std::move(figure));
    }
    for (const auto &figure : figures_)
        get_node<TextureRect>(String("BattlefieldScroll/Canvas/") + figure.node)
            ->set_tooltip_text(gs(figure.label));
}

void CombatSpriteDemo::refresh_players()
{
    for (unsigned variant = 0; variant < player_count; ++variant)
    {
        auto appearance = appearance_;
        appearance.tall = variant != 0;
        for (unsigned pose = 0; pose < 2; ++pose)
        {
            auto &figure = figures_[variant];
            const auto icon_pose =
                pose != 0 ? opengold::por::IconPose::action : opengold::por::IconPose::ready;
            figure.poses[pose] = presentation::image_texture(art_->icon(appearance, icon_pose));
            figure.visible_bounds[pose] = figure.poses[pose]->get_image()->get_used_rect();
        }
    }
    refresh_colors();
    refresh_figures();
}

void CombatSpriteDemo::refresh_colors()
{
    auto small = appearance_;
    small.tall = false;
    const auto tall_usage = art_->color_usage(appearance_), small_usage = art_->color_usage(small);
    const auto present = [&](unsigned bank, unsigned part)
    {
        return tall_usage.contains(bank, part) || small_usage.contains(bank, part);
    };
    if (!present(color_bank_, color_part_))
        for (unsigned i = 0; i < 12; ++i)
            if (present(i / 6, i % 6))
            {
                color_bank_ = i / 6;
                color_part_ = i % 6;
                break;
            }
    get_node<Label>("Head")->set_text(gs("Head ") + String::num_int64(appearance_.combat_head + 1) +
                                      " / 14");
    get_node<Label>("Body")->set_text(gs("Weapon ") +
                                      String::num_int64(appearance_.combat_body + 1) + " / 35");
    get_node<Label>("PaletteHint")
    ->set_text(gs(presentation::character_regions[color_part_]) + " / Color-" +
               String::num_int64(color_bank_ + 1) + ": choose a color");
    for (unsigned bank = 0; bank < 2; ++bank)
        for (unsigned part = 0; part < 6; ++part)
        {
            auto *button =
                get_node<Button>(gs("Color" + std::to_string(bank) + "_" + std::to_string(part)));
            const bool available = present(bank, part);
            button->set_disabled(!available);
            button->set_text(gs(available
                                ? presentation::character_colors[appearance_.colors[bank][part]]
                                : "Not present"));
            color_button(*button, appearance_.colors[bank][part],
                         color_bank_ == bank && color_part_ == part);
        }
    for (unsigned index = 0; index < 16; ++index)
        color_button(*get_node<Button>(gs("Palette" + std::to_string(index))), index,
                     appearance_.colors[color_bank_][color_part_] == index);
}

void CombatSpriteDemo::refresh_figures()
{
    auto *canvas = get_node<Control>("BattlefieldScroll/Canvas");
    canvas->call("set_board_dimensions", battlefield_.geometry.width,
                 battlefield_.geometry.height, zoom_);
    center_pending_ = true;
    const double scale = zoom_ / 100.0;
    get_node<Label>("Zoom")->set_text(gs("Zoom ") + String::num_int64(zoom_) + "%");
    get_node<Label>("Pose")->set_text(
        gs(action_ ? "Action pose · 1 second" : "Ready pose · 1 second"));
    for (const char *name :
            {"Minus100", "Minus10"
            })
        get_node<Button>(name)->set_disabled(zoom_ == min_zoom);
    for (const char *name :
            {"Plus100", "Plus10"
            })
        get_node<Button>(name)->set_disabled(zoom_ == max_zoom);
    String sizes = "[b]" + gs("Sprite dimensions — source → displayed pixels") + "[/b]\n";
    for (unsigned i = 0; i < figures_.size(); ++i)
    {
        const auto &figure = figures_[i];
        const auto &texture = figure.poses[action_];
        auto *sprite = get_node<TextureRect>(String("BattlefieldScroll/Canvas/") + figure.node);
        const Vector2 source(texture->get_width(), texture->get_height());
        const auto bounds = figure.visible_bounds[action_];
        Vector2 art_scale(1, 1);
        if (figure.sizing != Sizing::original && bounds.size.x > 0 && bounds.size.y > 0)
        {
            const double height_scale = goliath_height() * tile_pixels() / bounds.size.y;
            art_scale = Vector2(figure.sizing == Sizing::stretched ? tile_pixels() / bounds.size.x
                                : height_scale,
                                height_scale);
        }
        sprite->set_texture(texture);
        canvas->call("place_figure", figure.node, source, bounds, figure.cell,
                     static_cast<int>(figure.sizing), zoom_);
        const String color = figure_color(i).to_html(false);
        sizes += "[color=#" + color + "]" + gs(figure.label) + "[/color]  " + dimensions(source) +
                 gs(" → ") + dimensions(sprite->get_size()) + " px";
        if (i < player_count)
            sizes += gs("  · ") + gs("Visible figure: ") +
                     dimensions(bounds.size * art_scale * scale) + " px";
        sizes += "\n";
    }
    get_node<RichTextLabel>("Sizes")->set_text(sizes);
    canvas->queue_redraw();
}

void CombatSpriteDemo::draw_map()
{
    if (!loaded_)
        return;
    auto *canvas = get_node<Control>("BattlefieldScroll/Canvas");
    const double tile = tile_pixels() * zoom_ / 100.0;
    for (int y = 0; y < battlefield_.geometry.height; ++y)
        for (int x = 0; x < battlefield_.geometry.width; ++x)
        {
            const Rect2 cell(x * tile, y * tile, tile, tile);
            canvas->draw_texture_rect(
                terrain_.at(battlefield_.tiles[y * battlefield_.geometry.width + x]), cell, false);
            canvas->draw_rect(cell,
                              get_theme_color("combat_sprite_grid", "OpenGoldPalette"), false,
                              get_theme_constant("combat_sprite_grid_width", "OpenGoldMetrics"));
        }
    for (unsigned i = 0; i < figures_.size(); ++i)
    {
        const auto &figure = figures_[i];
        const auto color = figure_color(i);
        const Rect2 guide(figure.cell * tile, Vector2(tile, tile) * figure.footprint);
        canvas->draw_rect(guide, color, false,
                          get_theme_constant("combat_sprite_guide_width", "OpenGoldMetrics"));
        if (figure.sizing != Sizing::original)
        {
            // The lower quarter of the upper square is the target headroom.
            const Vector2 top = guide.position + Vector2(0, tile * (2 - goliath_height()));
            auto headroom = color;
            headroom.a = get_theme_constant("combat_sprite_headroom_percent", "OpenGoldMetrics") /
                         100.0;
            canvas->draw_rect(Rect2(top, Vector2(tile, tile * .25)), headroom);
            canvas->draw_line(top, top + Vector2(tile, 0), color,
                              get_theme_constant("combat_sprite_grid_width", "OpenGoldMetrics"));
            canvas->draw_line(guide.position + Vector2(0, tile),
                              guide.position + Vector2(tile, tile), color,
                              get_theme_constant("combat_sprite_grid_width", "OpenGoldMetrics"));
            canvas->draw_line(guide.position + Vector2(0, 2 * tile),
                              guide.position + Vector2(tile, 2 * tile), color,
                              get_theme_constant("combat_sprite_baseline_width", "OpenGoldMetrics"));
        }
        if (i < player_count && tile >= tile_pixels())
        {
            constexpr std::array<const char *, player_count> captions
            {
                "Short", "Human", "1 — Stretched", "2 — Proportional"};
            // draw_string's optional TextServer enums are omitted from the demo's trimmed bindings.
            canvas->call("draw_string", get_theme_default_font(),
                         guide.position + Vector2(-tile * .75, guide.size.y + 18), gs(captions[i]),
                         HORIZONTAL_ALIGNMENT_CENTER, tile * 2.5,
                         get_theme_constant("combat_sprite_caption_size", "OpenGoldMetrics"),
                         color);
        }
    }
}

void CombatSpriteDemo::zoom_by(int amount)
{
    if (!loaded_)
        return;
    auto *scroll = get_node<ScrollContainer>("BattlefieldScroll");
    if (!center_pending_)
        center_cell_ =
            (Vector2(scroll->get_h_scroll(), scroll->get_v_scroll()) + scroll->get_size() * .5) /
            (tile_pixels() * zoom_ / 100.0);
    zoom_ = std::clamp(zoom_ + amount, min_zoom, max_zoom);
    refresh_figures();
}

void CombatSpriteDemo::change_part(int part, int direction)
{
    if (!loaded_)
        return;
    auto &index = part == 0 ? appearance_.combat_head : appearance_.combat_body;
    const int count = part == 0 ? 14 : 35;
    index = (static_cast<int>(index) + direction + count) % count;
    refresh_players();
}

void CombatSpriteDemo::select_color(int bank, int part)
{
    if (loaded_)
    {
        color_bank_ = bank;
        color_part_ = part;
        refresh_colors();
    }
}

void CombatSpriteDemo::recolor(int index)
{
    if (loaded_)
    {
        appearance_.colors[color_bank_][color_part_] = index;
        refresh_players();
    }
}

void CombatSpriteDemo::_process(double delta)
{
    if (!loaded_ || !std::isfinite(delta) || delta < 0)
        return;
    if (center_pending_)
    {
        auto *canvas = get_node<Control>("BattlefieldScroll/Canvas");
        const auto minimum = canvas->get_custom_minimum_size();
        if (canvas->get_size().x >= minimum.x && canvas->get_size().y >= minimum.y)
        {
            auto *scroll = get_node<ScrollContainer>("BattlefieldScroll");
            const auto offset =
                center_cell_ * (tile_pixels() * zoom_ / 100.0) - scroll->get_size() * .5;
            scroll->set_h_scroll(std::max(0, int(offset.x)));
            scroll->set_v_scroll(std::max(0, int(offset.y)));
            center_pending_ = false;
        }
    }
    elapsed_ = std::fmod(elapsed_ + delta, 2.0);
    const bool action = elapsed_ >= 1;
    if (action != action_)
    {
        action_ = action;
        refresh_figures();
    }
}

void CombatSpriteDemo::_input(const Ref<InputEvent> &event)
{
    const Ref<InputEventKey> key = event;
    if (key.is_valid() && key->is_pressed() && !key->is_echo() && key->is_ctrl_pressed() &&
            key->get_keycode() == KEY_S)
    {
        get_viewport()->set_input_as_handled();
        request_capture();
    }
}

void CombatSpriteDemo::request_capture()
{
    if (capture_pending_)
        return;
    if (DisplayServer::get_singleton()->get_name() == "headless")
    {
        emit_signal("capture_completed", String(), "Screenshots require graphical rendering.");
        return;
    }
    capture_pending_ = true;
    RenderingServer::get_singleton()->connect(
        "frame_post_draw", callable_mp(this, &CombatSpriteDemo::capture_frame), CONNECT_ONE_SHOT);
}

void CombatSpriteDemo::capture_frame()
{
    capture_pending_ = false;
    auto directory = OS::get_singleton()->get_environment("OPENGOLD_SCREENSHOT_DIR");
    if (directory.is_empty())
        directory =
            ProjectSettings::get_singleton()->globalize_path("user://sprite-demo-screenshots");
    String path, error;
    if (!directory.is_absolute_path() || DirAccess::make_dir_recursive_absolute(directory) != OK)
        error = "Cannot create screenshots folder.";
    else
    {
        const auto stamp =
            Time::get_singleton()->get_datetime_string_from_system(true).replace(":", "-");
        path = directory.path_join(
                   stamp + gs("-") + String::num_int64(OS::get_singleton()->get_process_id()) + "-" +
                   String::num_uint64(Time::get_singleton()->get_ticks_usec()) + ".png");
        const auto image = get_viewport()->get_texture()->get_image();
        if (image.is_null() || image->save_png(path) != OK)
            error = "Cannot save screenshot.";
    }
    get_node<Label>("Status")->set_text(error.is_empty() ? gs("Screenshot saved (Ctrl+S).")
                                        : error);
    get_node<Label>("Status")->set_tooltip_text(path);
    emit_signal("capture_completed", path, error);
}
