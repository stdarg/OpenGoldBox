#ifndef OPENGOLDBOX_SCREENSHOT_SERVICE_H
#define OPENGOLDBOX_SCREENSHOT_SERVICE_H

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/array.hpp>
#include <exception>

// Presentation-only capture. The scene tree owns this autoload and its notice.
class ScreenshotService : public godot::Node
{
    GDCLASS(ScreenshotService, godot::Node)
  public:
    // A failed handler is already in the log, and this view has no status
    // line to show it; called by presentation::run_guarded.
    void report_failure(const std::exception &) noexcept
    {
    }

    void _ready() override;
    void _process(double delta) override;
    bool request_capture();

    godot::String get_directory() const
    {
        return directory_;
    }

  protected:
    static void _bind_methods();

  private:
    godot::String directory_, request_id_;
    bool pending_{}, file_request_{};
    double poll_time_{}, notice_time_{}, capture_wait_{};
    void capture_frame();
    void capture_windows(godot::Node &node, const godot::String &prefix, godot::Array &files,
                         godot::String &error);
    void complete(const godot::Array &files, godot::String error);
};

#endif
