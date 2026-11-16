#ifndef OPENGOLDBOX_MODIFY_CHARACTER_DIALOG_H
#define OPENGOLDBOX_MODIFY_CHARACTER_DIALOG_H
#include "opengold/character_rules.h"
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/window.hpp>
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <string>
#include <vector>

// The original games' Modify Character (docs/ORIGINAL-RULES.md): a new
// character's six scores and Hit Points, each row lowered or raised a step
// within the limits its rules set, from Curse on its name, and in Treasures
// a magic-user's spell book chosen again. Up and Down choose a row, Left and
// Right lower or raise it, Enter keeps the changes and Escape leaves them;
// the mouse uses the rows' buttons. The controls come from the creation
// scene, in containers.
class ModifyCharacterDialog : public godot::Window
{
    GDCLASS(ModifyCharacterDialog, godot::Window)
  public:
    // What Keep hands back: the draft with the changes kept.
    using Keep = std::function<void(opengold::rules::CharacterDraft)>;

    struct Request
    {
        std::shared_ptr<const opengold::rules::CharacterRules> rules;
        opengold::rules::CharacterDraft draft;
        // Whether the name may change here (rules::CharacterRules::modify_renames).
        bool renames{};
        // Names the character may not take: Unlimited Adventures refuses
        // another member's.
        std::vector<std::string> taken_names;
        Keep keep;
    };

    void _ready() override;
    // Shows the draft's numbers and the dialog.
    void open(Request request);
    // Shows a failed handler's error; called by presentation::run_guarded.
    void report_failure(const std::exception &failure);

    // The row the keyboard changes: 0 to 5 the scores in the order of
    // rules::Ability, 6 the Hit Points.
    [[nodiscard]] int selected_row() const noexcept
    {
        return row_;
    }

  protected:
    static void _bind_methods()
    {
    }

  private:
    void connect_controls();
    void window_input(const godot::Ref<godot::InputEvent> &event);
    void select_row(std::int64_t row);
    void step(std::int64_t row, bool raise);
    void name_changed(const godot::String &name);
    void spell_toggled(bool selected, const godot::String &group, const godot::String &option);
    void keep();
    void close();
    void refresh();
    // The spell choices to make again (Treasures' magic-user's), rebuilt
    // when the dialog opens.
    void fill_spells();
    // Why Keep waits, or nothing when it does not.
    [[nodiscard]] godot::String keep_refusal() const;
    Request request_;
    int row_{};
};
#endif
