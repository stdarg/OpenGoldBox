#ifndef OPENGOLD_SOUND_BOARD_VIEW_H
#define OPENGOLD_SOUND_BOARD_VIEW_H
#include "opengold/sound_player.h"
#include <godot_cpp/classes/control.hpp>

class SoundBoardView : public godot::Control
{
    GDCLASS(SoundBoardView, godot::Control)
  public:
    void _ready() override;
    void _exit_tree() override;
    void _process(double delta) override;

  protected:
    static void _bind_methods();

  private:
    std::unique_ptr<opengold::por::SoundPlayer> audio_;
    std::unique_ptr<opengold::por::SoundOutput> teardown_check_output_;
    bool checking_{}, capture_{}, captured_{}, loaded_{};
    bool check_finishing_{}, check_passed_{};
    int selected_{-1}, check_index_{-1}, frames_{};
    double check_elapsed_{};
    void play(int index);
    void stop();
    void finished();
    void volume(double value);
    void mute(bool value);
    void refresh_buttons();
};
#endif
