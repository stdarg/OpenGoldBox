#ifndef OPENGOLD_SAVE_SLOTS_H
#define OPENGOLD_SAVE_SLOTS_H
#include <godot_cpp/classes/window.hpp>
#include <exception>
#include <filesystem>
#include <functional>
#include <vector>

// Scene-owned shared save UI. Callbacks remain in the campaign host.
class SaveSlots : public godot::Window
{
    GDCLASS(SaveSlots, godot::Window)
  public:
    void _ready() override;
    void open(bool saving);
    // Shows a failed handler's error; called by presentation::run_guarded.
    void report_failure(const std::exception &failure);
    using FileAction = std::function<void(const std::filesystem::path &)>;
    // The campaign host's save and load, set once; both are required.
    void connect_host(FileAction save, FileAction load);

  protected:
    static void _bind_methods()
    {
    }

  private:
    // Private, so nothing else can replace or call them (Effective C++
    // Item 22).
    FileAction save_, load_;
    bool saving_{}, confirmed_{};
    std::filesystem::path directory_, pending_;
    std::vector<std::filesystem::path> paths_;
    void select(std::int64_t index);
    void changed(godot::String text);
    void act();
    void close();
};
#endif
