#include "screenshot_service.h"
#include "godot_nodes.h"
#include "localization.h"
#include "guarded_handlers.h"
#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/viewport_texture.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <algorithm>

using namespace godot;
using presentation::required_node;

void ScreenshotService::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("request_capture"), &ScreenshotService::request_capture);
    ClassDB::bind_method(D_METHOD("get_directory"), &ScreenshotService::get_directory);
    ADD_SIGNAL(MethodInfo("capture_completed", PropertyInfo(Variant::ARRAY, "files"),
                          PropertyInfo(Variant::STRING, "error")));
}

void ScreenshotService::_ready()
{
    set_process_mode(PROCESS_MODE_ALWAYS);
    const auto override = OS::get_singleton()->get_environment("OPENGOLD_SCREENSHOT_DIR");
    directory_ = override.is_empty()
                 ? ProjectSettings::get_singleton()->globalize_path("user://screenshots")
                 : override;
    if (!directory_.is_absolute_path())
    {
        UtilityFunctions::printerr("OPENGOLD_SCREENSHOT_DIR must be an absolute path");
        directory_ = String();
    }
    else
    {
        DirAccess::make_dir_recursive_absolute(directory_);
        UtilityFunctions::print("OpenGoldBox screenshots: ", directory_);
    }
    auto layer = presentation::make_node<CanvasLayer>();
    layer->set_name("NoticeLayer");
    layer->set_layer(100);
    auto *canvas = presentation::attach_child(*this, std::move(layer));
    auto *panel = presentation::add_control<PanelContainer>(*canvas, "Notice", {});
    panel->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
    panel->hide();
    Ref<StyleBoxFlat> style;
    style.instantiate();
    style->set_bg_color(Color(.04, .05, .08, .96));
    for (auto side :
            {
                SIDE_LEFT, SIDE_TOP, SIDE_RIGHT, SIDE_BOTTOM
            })
    {
        style->set_content_margin(side, 14);
        style->set_border_width(side, 1);
    }
    style->set_border_color(Color(.7, .6, .4));
    panel->add_theme_stylebox_override("panel", style);
    auto *label = presentation::add_control<Label>(*panel, "Text", {});
    label->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
    label->set_auto_translate_mode(Node::AUTO_TRANSLATE_MODE_DISABLED);
    label->set("autowrap_mode",
               3); // TextServer::AUTOWRAP_WORD_SMART; matches the game build profile.
}

void ScreenshotService::_process(double delta)
{
    auto *panel = &required_node<PanelContainer>(*this, "NoticeLayer/Notice"); // scene-owned
    if (notice_time_ > 0)
    {
        notice_time_ -= delta;
        const auto size = get_tree()->get_root()->get_size();
        const Vector2 minimum(std::max(1, std::min(800, size.x - 40)), 0);
        panel->set_custom_minimum_size(minimum);
        panel->set_size(minimum);
        const double bottom = static_cast<double>(size.y - panel->get_size().y - 20);
        panel->set_position(Vector2(20, std::max(0.0, bottom)));
        if (notice_time_ <= 0)
            panel->hide();
    }
    poll_time_ += delta;
    if (pending_)
    {
        capture_wait_ += delta;
        if (capture_wait_ > 5)
        {
            RenderingServer::get_singleton()->disconnect(
                "frame_post_draw", presentation::guarded(this, &ScreenshotService::capture_frame));
            complete({}, i18n::text("Cannot capture the current game frame."));
        }
    }
    if (poll_time_ < .1 || pending_ || directory_.is_empty())
        return;
    poll_time_ = 0;
    const auto request = directory_.path_join("capture.request");
    if (!FileAccess::file_exists(request))
        return;
    {
        const auto input = FileAccess::open(request, FileAccess::READ);
        if (input.is_null() || input->get_length() > 128)
            return;
        request_id_ = input->get_as_text().strip_edges();
    }
    file_request_ = true;
    request_capture();
}

bool ScreenshotService::request_capture()
{
    if (pending_ || !is_inside_tree())
        return false;
    pending_ = true;
    capture_wait_ = 0;
    required_node<PanelContainer>(*this, "NoticeLayer/Notice").hide();
    notice_time_ = 0;
    if (directory_.is_empty() || DirAccess::make_dir_recursive_absolute(directory_) != OK ||
            !DirAccess::dir_exists_absolute(directory_))
    {
        complete({}, i18n::text("Cannot create the screenshots folder."));
        return true;
    }
    if (DisplayServer::get_singleton()->get_name() == "headless")
    {
        complete({}, i18n::text("Screenshots require a running graphical game window."));
        return true;
    }
    // Read back the next completed frame, after any preceding notice is hidden.
    const auto error = RenderingServer::get_singleton()->connect(
                           "frame_post_draw", presentation::guarded(this, &ScreenshotService::capture_frame),
                           Object::CONNECT_ONE_SHOT);
    if (error != OK)
        complete({}, i18n::text("Cannot capture the current game frame."));
    return true;
}

void ScreenshotService::capture_windows(Node &node, const String &prefix, Array &files,
                                        String &error)
{
    auto *window = Object::cast_to<Window>(&node); // borrowed for this synchronous capture
    if (window && window->is_visible())
    {
        const auto texture = window->get_texture();
        const auto image = texture.is_valid() ? texture->get_image() : Ref<Image> {};
        const bool main = window == get_tree()->get_root();
        const auto path =
            prefix +
            (main ? String("-main.png")
             : String("-window-") + String::num_uint64(window->get_instance_id()) + ".png");
        if (image.is_null() || image->is_empty())
        {
            error = i18n::text("Cannot capture the current game frame.");
            return;
        }
        if (image->save_png(path) != OK)
        {
            DirAccess::remove_absolute(path);
            error = i18n::format("Cannot save screenshot: {path}", {{"path", path}});
            return;
        }
        Dictionary file;
        file["path"] = path;
        file["window"] = window->get_name();
        file["width"] = image->get_width();
        file["height"] = image->get_height();
        file["main"] = main;
        files.push_back(file);
    }
    for (int i = 0; i < node.get_child_count(); ++i)
    {
        capture_windows(*node.get_child(i), prefix, files, error);
        if (!error.is_empty())
            return;
    }
}

void ScreenshotService::capture_frame()
{
    const auto stamp =
        Time::get_singleton()->get_datetime_string_from_system(true).replace(":", "-") + "Z";
    const auto process = String::num_int64(OS::get_singleton()->get_process_id());
    String prefix;
    do
    {
        prefix = directory_.path_join(stamp + String("-") + process + String("-") +
                                      String::num_uint64(Time::get_singleton()->get_ticks_usec()));
    }
    while (FileAccess::file_exists(prefix + "-main.png"));
    Array files;
    String error;
    capture_windows(*get_tree()->get_root(), prefix, files, error);
    if (files.is_empty() && error.is_empty())
        error = i18n::text("Cannot capture the current game frame.");
    complete(files, error);
}

void ScreenshotService::complete(const Array &files, String error)
{
    // Publish a small manifest last, so tools can find this capture's PNGs and
    // distinguish a completed capture from an old image or a failed request.
    Dictionary result;
    result["files"] = files;
    result["error"] = error;
    result["ok"] = error.is_empty();
    result["request_id"] = request_id_;
    const auto temporary = directory_.path_join("latest.json.tmp");
    bool published = false;
    if (!directory_.is_empty())
    {
        {
            const auto output = FileAccess::open(temporary, FileAccess::WRITE);
            if (output.is_valid())
            {
                output->store_string(JSON::stringify(result));
                output->flush();
                published = output->get_error() == OK;
            }
        }
        if (published)
            published =
                DirAccess::rename_absolute(temporary, directory_.path_join("latest.json")) == OK;
        if (!published)
            DirAccess::remove_absolute(temporary);
    }
    if (!published && error.is_empty())
        error = i18n::text("Screenshots saved, but the capture report could not be written.");
    if (file_request_)
        DirAccess::remove_absolute(directory_.path_join("capture.request"));
    file_request_ = false;
    request_id_ = String();
    const auto message = error.is_empty()
    ? i18n::format("Screenshot saved to {path}", {{"path", directory_}})
        : error;
    required_node<Label>(*this, "NoticeLayer/Notice/Text").set_text(message);
    required_node<PanelContainer>(*this, "NoticeLayer/Notice").show();
    notice_time_ = 5;
    if (error.is_empty())
        UtilityFunctions::print(message);
    else
        UtilityFunctions::printerr(message);
    pending_ = false;
    emit_signal("capture_completed", files, error);
}
