#include "save_slots.h"
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/item_list.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/line_edit.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <algorithm>
#include <memory>
#include <stdexcept>
using namespace godot;

namespace
{
struct DeleteNode
{
    void operator()(Node *n) const
    {
        memdelete(n);
    }
};

template <class T> void add(Window &parent, const char *name, Rect2 rect)
{
    std::unique_ptr<T, DeleteNode> child(memnew(T));
    child->set_name(name);
    child->set_position(rect.position);
    child->set_size(rect.size);
    parent.add_child(child.get());
    child.release();
}

std::string encoded(std::string_view name)
{
    static constexpr char hex[] = "0123456789abcdef";
    std::string result;
    for (unsigned char c : name)
    {
        result += hex[c >> 4];
        result += hex[c & 15];
    }
    return result;
}

std::string decoded(std::string_view stem)
{
    std::string result;
    if (stem.size() % 2)
        return {};
    for (std::size_t i = 0; i < stem.size(); i += 2)
    {
        const auto digit = [](char c)
        {
            return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
        };
        int a = digit(stem[i]), b = digit(stem[i + 1]);
        if (a < 0 || b < 0)
            return {};
        result += static_cast<char>(a * 16 + b);
    }
    return result;
}
} // namespace

void SaveSlots::_ready()
{
    set_size(Vector2i(620, 470));
    set_min_size(Vector2i(620, 470));
    set_flag(Window::FLAG_RESIZE_DISABLED, true);
    set_exclusive(true);
    set_transient(true);
    add<Label>(*this, "Help", Rect2(20, 16, 580, 42));
    add<ItemList>(*this, "Slots", Rect2(20, 66, 580, 230));
    add<LineEdit>(*this, "Name", Rect2(20, 310, 580, 36));
    add<Label>(*this, "Status", Rect2(20, 354, 580, 58));
    add<Button>(*this, "Action", Rect2(300, 420, 145, 36));
    add<Button>(*this, "Cancel", Rect2(455, 420, 145, 36));
    get_node<Label>("Status")->set("autowrap_mode", 3);
    get_node<LineEdit>("Name")->set_placeholder("Save name");
    get_node<LineEdit>("Name")->set_max_length(60);
    get_node<ItemList>("Slots")->connect("item_selected", callable_mp(this, &SaveSlots::select));
    get_node<LineEdit>("Name")->connect("text_changed", callable_mp(this, &SaveSlots::changed));
    get_node<Button>("Action")->connect("pressed", callable_mp(this, &SaveSlots::act));
    get_node<Button>("Cancel")->set_text("Cancel");
    get_node<Button>("Cancel")->connect("pressed", callable_mp(this, &SaveSlots::close));
    connect("close_requested", callable_mp(this, &SaveSlots::close));
    hide();
    directory_ = std::filesystem::u8path(
                     ProjectSettings::get_singleton()->globalize_path("user://saves").utf8().get_data());
}

void SaveSlots::open(bool saving)
{
    saving_ = saving;
    confirmed_ = false;
    pending_.clear();
    paths_.clear();
    set_title(saving ? "Save game" : "Load game");
    get_node<Label>("Help")->set_text(
        saving ? "Select a save to overwrite, or enter a new name."
        : "Select a save. Previous versions are available for recovery.");
    auto *list = get_node<ItemList>("Slots");
    list->clear();
    try
    {
        if (std::filesystem::exists(directory_))
            for (auto &e : std::filesystem::directory_iterator(directory_))
            {
                if (!e.is_regular_file())
                    continue;
                auto p = e.path();
                bool backup = p.extension() == ".bak";
                if (backup)
                    p = p.stem();
                if (p.extension() != ".ogs" || (saving && backup))
                    continue;
                auto name = decoded(p.stem().string());
                if (name.empty())
                    continue;
                paths_.push_back(e.path());
            }
        std::sort(paths_.begin(), paths_.end());
        for (auto &p : paths_)
        {
            bool backup = p.extension() == ".bak";
            auto base = backup ? p.stem() : p;
            auto name = decoded(base.stem().string()) + (backup ? " (previous version)" : "");
            list->add_item(String::utf8(name.c_str()));
        }
        get_node<Label>("Status")->set_text("");
    }
    catch (const std::exception &e)
    {
        get_node<Label>("Status")->set_text(String::utf8(e.what()));
    }
    get_node<LineEdit>("Name")->set_text("");
    get_node<LineEdit>("Name")->set_visible(saving);
    get_node<Button>("Action")->set_text(saving ? "Save" : "Load");
    popup_centered();
    if (saving)
        get_node<LineEdit>("Name")->grab_focus();
    else
        list->grab_focus();
}

void SaveSlots::changed(String)
{
    confirmed_ = false;
    pending_.clear();
    get_node<Button>("Action")->set_text(saving_ ? "Save" : "Load");
}

void SaveSlots::select(std::int64_t index)
{
    changed("");
    if (index < 0 || static_cast<std::size_t>(index) >= paths_.size())
        return;
    if (saving_)
        get_node<LineEdit>("Name")->set_text(
            String::utf8(decoded(paths_[index].stem().string()).c_str()));
}

void SaveSlots::act()
{
    try
    {
        std::filesystem::path path;
        if (saving_)
        {
            auto text = get_node<LineEdit>("Name")->get_text().strip_edges();
            std::string name = text.utf8().get_data();
            if (name.empty() || name.size() > 120)
                throw std::runtime_error("Enter a save name of at most 120 UTF-8 bytes.");
            path = directory_ / (encoded(name) + ".ogs");
        }
        else
        {
            auto selected = get_node<ItemList>("Slots")->get_selected_items();
            if (selected.is_empty())
                throw std::runtime_error("Select a save first.");
            path = paths_.at(selected[0]);
        }
        if ((!saving_ || std::filesystem::exists(path)) && (!confirmed_ || pending_ != path))
        {
            confirmed_ = true;
            pending_ = path;
            get_node<Label>("Status")->set_text(
                saving_ ? "Overwrite this save? Its previous version will be retained."
                : "Load this save? Any unsaved campaign progress will be discarded.");
            get_node<Button>("Action")->set_text(saving_ ? "Overwrite" : "Confirm load");
            return;
        }
        if (saving_)
            save(path);
        else
            load(path);
        hide();
    }
    catch (const std::exception &e)
    {
        confirmed_ = false;
        get_node<Label>("Status")->set_text(String::utf8(e.what()));
        get_node<Button>("Action")->set_text(saving_ ? "Save" : "Load");
    }
}

void SaveSlots::close()
{
    hide();
}
