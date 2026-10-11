#ifndef OPENGOLDBOX_GODOT_NODES_H
#define OPENGOLDBOX_GODOT_NODES_H

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/variant/packed_float64_array.hpp>
#include <algorithm>
#include <initializer_list>
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

// Restore a scene-authored rectangle after a state-specific layout has moved
// a control. Cache offsets, not pixels, so Godot's anchors remain authoritative
// when the parent changes size.
inline void restore_scene_control(godot::Control &node)
{
    const godot::StringName key("_scene_offsets");
    if (!node.has_meta(key))
    {
        godot::PackedFloat64Array offsets;
        for (auto side : {godot::SIDE_LEFT, godot::SIDE_TOP, godot::SIDE_RIGHT, godot::SIDE_BOTTOM})
            offsets.push_back(node.get_offset(side));
        node.set_meta(key, offsets);
    }
    const godot::PackedFloat64Array offsets = node.get_meta(key);
    godot::Vector2 parent_size;
    if (auto *parent = godot::Object::cast_to<godot::Control>(node.get_parent()))
        parent_size = parent->get_size();
    else if (auto *parent = godot::Object::cast_to<godot::Window>(node.get_parent()))
        parent_size = parent->get_size();
    else
        throw std::runtime_error("Scene control has no layout parent");
    godot::Vector2 start(node.get_anchor(godot::SIDE_LEFT) * parent_size.x + offsets[0],
                         node.get_anchor(godot::SIDE_TOP) * parent_size.y + offsets[1]);
    godot::Vector2 end(node.get_anchor(godot::SIDE_RIGHT) * parent_size.x + offsets[2],
                       node.get_anchor(godot::SIDE_BOTTOM) * parent_size.y + offsets[3]);
    if (node.has_meta("layout_middle_delta"))
    {
        const double small = node.get_theme_constant("layout_small_width", "OpenGoldMetrics");
        const double middle = node.get_theme_constant("layout_middle_width", "OpenGoldMetrics");
        const double design = node.get_theme_constant("layout_design_width", "OpenGoldMetrics");
        const double factor = parent_size.x <= middle
                              ? std::clamp((parent_size.x - small) / (middle - small), 0.0, 1.0)
                              : std::clamp((design - parent_size.x) / (design - middle), 0.0, 1.0);
        const godot::Rect2 delta = node.get_meta("layout_middle_delta");
        start += delta.position * factor;
        end += (delta.position + delta.size) * factor;
    }
    node.set_position(start);
    node.set_size(end - start);
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

// Dialog scenes own their controls and rectangles. Existing native window
// classes attach those authored children before connecting their behavior.
[[nodiscard]] inline NodeOwner<> dialog_layout_scene(const godot::String &group)
{
    const godot::String path =
        godot::String("res://scenes/dialog_layouts/") + group + godot::String(".tscn");
    const bool exists = godot::ResourceLoader::get_singleton()->exists(path);
    if (!exists)
    {
        throw std::runtime_error("Missing dialog layout scene: " +
                                 std::string(path.utf8().get_data()));
    }
    godot::Ref<godot::PackedScene> packed = godot::ResourceLoader::get_singleton()->load(path);
    if (packed.is_null())
        throw std::runtime_error("Cannot load dialog layout scene: " +
                                 std::string(path.utf8().get_data()));
    return NodeOwner<>(packed->instantiate());
}

[[nodiscard]] inline godot::Rect2 dialog_layout_rect_group(const godot::String &group,
                                                           const godot::String &name)
{
    auto guides = dialog_layout_scene(group);
    if (!guides)
        throw std::runtime_error("Missing dialog layout scene: " +
                                 std::string(group.utf8().get_data()));
    auto &guide = required_node<godot::Control>(*guides, godot::NodePath(name));
    return {guide.get_position(), guide.get_size()};
}

[[nodiscard]] inline godot::Vector2i dialog_layout_size_group(const godot::String &group)
{
    auto guides = dialog_layout_scene(group);
    if (!guides)
        throw std::runtime_error("Missing dialog layout scene: " +
                                 std::string(group.utf8().get_data()));
    auto *guide = godot::Object::cast_to<godot::Control>(guides.get());
    if (!guide)
        throw std::runtime_error("Invalid dialog layout root: " +
                                 std::string(group.utf8().get_data()));
    return godot::Vector2i(guide->get_size());
}

inline void attach_dialog_layout(godot::Window &window)
{
    auto scene = dialog_layout_scene(godot::String(window.get_name()));
    auto *layout = godot::Object::cast_to<godot::Control>(scene.get());
    if (!layout)
        throw std::runtime_error("Invalid dialog layout root");
    const godot::Vector2i configured(layout->get_size());
    window.set_size(configured);
    window.set_min_size(configured);
    while (layout->get_child_count() > 0)
    {
        auto *child = godot::Object::cast_to<godot::Control>(layout->get_child(0));
        if (!child)
            throw std::runtime_error("Invalid dialog layout child");
        child->set_owner(nullptr);
        attach_child(window, detach_child(*layout, *child));
    }
}

template <class T> T *dialog_control(godot::Node &parent, const godot::String &name)
{
    return &required_node<T>(parent, godot::NodePath(name));
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
