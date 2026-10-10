#ifndef OPENGOLDBOX_GODOT_NODES_H
#define OPENGOLDBOX_GODOT_NODES_H

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/window.hpp>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace presentation
{
struct DeleteNode
{
    void operator()(godot::Node *node) const noexcept
    {
        memdelete(node);
    }
};
// Own only detached nodes. Once attached, Godot owns the entire subtree.
template <class T = godot::Node> using NodeOwner = std::unique_ptr<T, DeleteNode>;

template <class T> [[nodiscard]] NodeOwner<T> make_node()
{
    return NodeOwner<T>(memnew(T));
}

// The returned pointer borrows the parent's child; callers must not delete it.
template <class T> T *attach_child(godot::Node &parent, NodeOwner<T> child)
{
    if (!child || child->get_parent())
        throw std::logic_error("Expected a detached child");
    parent.add_child(child.get());
    if (child->get_parent() != &parent)
        throw std::runtime_error("Cannot attach child node");
    return child.release();
}

// Takes a child back from its parent: the returned owner frees it unless it is
// attached again. The reverse of attach_child, in one step, so nothing can come
// between leaving the tree and being owned (Effective C++ Item 13).
template <class T> [[nodiscard]] NodeOwner<T> detach_child(godot::Node &parent, T &child)
{
    if (child.get_parent() != &parent)
        throw std::logic_error("Expected a child of this parent");
    parent.remove_child(&child);
    return NodeOwner<T>(&child);
}

// Node::get_node<T> returns null for a missing or differently typed node, and
// nothing makes its caller check. required_node reports the path instead, as
// an error a guarded handler shows rather than a crash, and returns a
// reference so callers can see the node is always there (Effective C++ Item 18).
template <class T>
[[nodiscard]] T &required_node(const godot::Node &parent, const godot::NodePath &path)
{
    auto *node = godot::Object::cast_to<T>(parent.get_node_or_null(path));
    if (!node)
        throw std::runtime_error("Missing or mistyped node: " +
                                 std::string(godot::String(path).utf8().get_data()));
    return *node;
}

// A scene owns the editable rectangle at the design size. Existing view
// calculations supply only the change caused by window size or live state.
// layout_reference is the view's rectangle at the design size; editing the
// control's offsets in the scene therefore needs no native rebuild.
[[nodiscard]] inline godot::Rect2 scene_layout_rect(godot::Control &node,
                                                    const godot::Rect2 &calculated)
{
    const godot::StringName reference_key("layout_reference");
    if (!node.has_meta(reference_key))
        return calculated;
    const godot::StringName initial_key("_layout_initial_rect");
    if (!node.has_meta(initial_key))
        node.set_meta(initial_key, godot::Rect2(node.get_position(), node.get_size()));
    const godot::Rect2 reference = node.get_meta(reference_key);
    const godot::Rect2 initial = node.get_meta(initial_key);
    return godot::Rect2(initial.position + calculated.position - reference.position,
                        initial.size + calculated.size - reference.size);
}

inline void place_scene_control(godot::Control &node, const godot::Rect2 &calculated)
{
    const auto rect = scene_layout_rect(node, calculated);
    node.set_position(rect.position);
    node.set_size(rect.size);
}

inline void position_scene_control(godot::Control &node, const godot::Vector2 &calculated)
{
    const auto rect = scene_layout_rect(node, godot::Rect2(calculated, node.get_size()));
    node.set_position(rect.position);
}

inline void size_scene_control(godot::Control &node, const godot::Vector2 &calculated)
{
    const auto rect = scene_layout_rect(node, godot::Rect2(node.get_position(), calculated));
    node.set_size(rect.size);
}

inline void size_scene_window(godot::Window &window, const godot::Vector2i &calculated)
{
    const godot::StringName reference_key("layout_reference_size");
    if (!window.has_meta(reference_key))
    {
        window.set_size(calculated);
        return;
    }
    const godot::StringName initial_key("_layout_initial_size");
    if (!window.has_meta(initial_key))
        window.set_meta(initial_key, window.get_size());
    const godot::Vector2i reference = window.get_meta(reference_key);
    const godot::Vector2i initial = window.get_meta(initial_key);
    window.set_size(initial + calculated - reference);
}

[[nodiscard]] inline NodeOwner<> instantiate_scene(const char *path)
{
    godot::Ref<godot::PackedScene> packed = godot::ResourceLoader::get_singleton()->load(path);
    if (packed.is_null())
        throw std::runtime_error(std::string("Missing scene: ") + path);
    NodeOwner<> scene(packed->instantiate());
    if (!scene)
        throw std::runtime_error(std::string("Cannot instantiate scene: ") + path);
    return scene;
}

// Some existing dialogs are assembled in C++. Their editable rectangles live
// in Godot scenes alongside the main screen layouts.
[[nodiscard]] inline NodeOwner<> dialog_layout_scene(const godot::String &group)
{
    const godot::String path =
        godot::String("res://scenes/dialog_layouts/") + group + godot::String(".tscn");
    const bool exists = godot::ResourceLoader::get_singleton()->exists(path);
    if (!exists)
    {
        for (const char *required : {"Defeat", "EquipmentChoice", "InitiativeChoice",
                                     "NickAttack", "OptionalEffect", "OptionalEffectMultiple",
                                     "RestDialog", "RestDialogOptions", "RestSpells",
                                     "RestTraining", "SaveSlots"})
            if (group == godot::String(required))
                throw std::runtime_error("Missing dialog layout scene: " +
                                         std::string(path.utf8().get_data()));
        return {};
    }
    godot::Ref<godot::PackedScene> packed = godot::ResourceLoader::get_singleton()->load(path);
    if (packed.is_null())
        throw std::runtime_error("Cannot load dialog layout scene: " +
                                 std::string(path.utf8().get_data()));
    return NodeOwner<>(packed->instantiate());
}

[[nodiscard]] inline godot::Rect2 dialog_layout_rect_group(const godot::String &group,
                                                           const godot::String &name,
                                                           const godot::Rect2 &fallback)
{
    auto guides = dialog_layout_scene(group);
    if (!guides)
        return fallback;
    auto *guide = godot::Object::cast_to<godot::Control>(guides->get_node_or_null(name));
    if (!guide)
        throw std::runtime_error(
            "Missing dialog layout control: " + std::string(group.utf8().get_data()) + "/" +
            std::string(name.utf8().get_data()));
    return godot::Rect2(guide->get_position(), guide->get_size());
}

[[nodiscard]] inline godot::Rect2 dialog_layout_rect(const godot::Node &parent,
                                                     const godot::String &name,
                                                     const godot::Rect2 &fallback)
{
    return dialog_layout_rect_group(godot::String(parent.get_name()), name, fallback);
}

[[nodiscard]] inline godot::Vector2i dialog_layout_size_group(const godot::String &group,
                                                              const godot::Vector2i &fallback)
{
    auto guides = dialog_layout_scene(group);
    auto *guide = guides ? godot::Object::cast_to<godot::Control>(guides.get()) : nullptr;
    return guide ? godot::Vector2i(guide->get_size()) : fallback;
}

[[nodiscard]] inline godot::Vector2i dialog_layout_size(const godot::Node &window,
                                                        const godot::Vector2i &fallback)
{
    return dialog_layout_size_group(godot::String(window.get_name()), fallback);
}

inline void set_dialog_window_size(godot::Window &window, const godot::Vector2i &fallback)
{
    const auto configured = dialog_layout_size(window, fallback);
    window.set_size(configured);
    window.set_min_size(configured);
}

template <class T> T *add_control(godot::Node &parent, const godot::String &name, godot::Rect2 rect)
{
    auto child = make_node<T>();
    child->set_name(name);
    const auto configured = dialog_layout_rect(parent, name, rect);
    child->set_position(configured.position);
    child->set_size(configured.size);
    return attach_child(parent, std::move(child));
}
// Blocks an object's signals while a control is refilled, and restores them
// at unblock() or, if an exception skips that, when the scope ends, so a
// failure cannot leave a dropdown silent (Effective C++ Item 13).
class SignalsBlocked
{
  public:
    explicit SignalsBlocked(godot::Object &object)
        : object_(&object), previous_(object.is_blocking_signals())
    {
        object.set_block_signals(true);
    }

    SignalsBlocked(const SignalsBlocked &) = delete;
    SignalsBlocked &operator=(const SignalsBlocked &) = delete;

    ~SignalsBlocked()
    {
        unblock();
    }

    void unblock() noexcept
    {
        if (!object_)
            return;
        object_->set_block_signals(previous_);
        object_ = nullptr;
    }

  private:
    godot::Object *object_;
    bool previous_;
};
} // namespace presentation
#endif
