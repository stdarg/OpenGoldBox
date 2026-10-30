#ifndef OPENGOLD_COMBAT_TEST_BRIDGE_H
#define OPENGOLD_COMBAT_TEST_BRIDGE_H
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/dictionary.hpp>

class CombatTestBridge : public godot::Node
{
    GDCLASS(CombatTestBridge, godot::Node)
  public:
    [[nodiscard]] godot::Dictionary catalog() const;
    [[nodiscard]] godot::Dictionary spell_options(const godot::Dictionary &member) const;
    [[nodiscard]] godot::String start(const godot::Dictionary &configuration);
    [[nodiscard]] godot::String outcome() const;
    void leave();

  protected:
    static void _bind_methods();
};
#endif
