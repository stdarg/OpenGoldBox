#ifndef OPENGOLDBOX_GODOT_NODES_H
#define OPENGOLDBOX_GODOT_NODES_H

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
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

template <class T> T *add_control(godot::Node &parent, const godot::String &name, godot::Rect2 rect)
{
    auto child = make_node<T>();
    child->set_name(name);
    child->set_position(rect.position);
    child->set_size(rect.size);
    return attach_child(parent, std::move(child));
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
} // namespace presentation
#endif
