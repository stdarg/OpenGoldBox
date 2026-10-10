#include "godot_nodes.h"
#include "localization.h"
#include "save_slots.h"
#include "godot_path.h"
#include "guarded_handlers.h"
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
using presentation::required_node;

namespace
{

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
    presentation::set_dialog_window_size(*this);
    set_flag(Window::FLAG_RESIZE_DISABLED, true);
    set_exclusive(true);
    set_transient(true);
    presentation::add_control<Label>(*this, "Help");
    presentation::add_control<ItemList>(*this, "Slots");
    presentation::add_control<LineEdit>(*this, "Name");
    presentation::add_control<Label>(*this, "Status");
    presentation::add_control<Button>(*this, "Action");
    presentation::add_control<Button>(*this, "Cancel");
    required_node<Label>(*this, "Help").set("autowrap_mode", 3);
    required_node<Label>(*this, "Status").set("autowrap_mode", 3);
    required_node<LineEdit>(*this, "Name").set_placeholder(i18n::text(N_("Save name")));
    required_node<LineEdit>(*this, "Name").set_max_length(60);
    required_node<ItemList>(*this, "Slots")
    .set_auto_translate_mode(Node::AUTO_TRANSLATE_MODE_DISABLED);
    required_node<ItemList>(*this, "Slots").connect("item_selected", presentation::guarded(this,
            &SaveSlots::select));
    required_node<LineEdit>(*this, "Name").connect("text_changed", presentation::guarded(this,
            &SaveSlots::changed));
    required_node<Button>(*this, "Action").connect("pressed", presentation::guarded(this,
            &SaveSlots::act));
    required_node<Button>(*this, "Cancel").set_text(i18n::text(N_("Cancel")));
    required_node<Button>(*this, "Cancel").connect("pressed", presentation::guarded(this,
            &SaveSlots::close));
    connect("close_requested", presentation::guarded(this, &SaveSlots::close));
    hide();
    directory_ = presentation::path_from_godot(
                     ProjectSettings::get_singleton()->globalize_path("user://saves"));
}

void SaveSlots::connect_host(FileAction save, FileAction load)
{
    if (!save || !load)
        throw std::logic_error("Save slots need both a save and a load action");
    save_ = std::move(save);
    load_ = std::move(load);
}

void SaveSlots::use_character_directory(std::filesystem::path directory)
{
    if (directory.empty())
        throw std::invalid_argument("Character save directory is empty");
    directory_ = std::move(directory);
    character_saves_ = true;
}

void SaveSlots::report_failure(const std::exception &failure)
{
    required_node<Label>(*this, "Status").set_text(i18n::text(failure.what()));
}

void SaveSlots::open(bool saving)
{
    saving_ = saving;
    confirmed_ = false;
    pending_.clear();
    paths_.clear();
    set_title(i18n::text(character_saves_
                         ? (saving ? N_("Save character") : N_("Load character"))
                         : (saving ? N_("Save game") : N_("Load game"))));
    required_node<Label>(*this, "Help").set_text(
        i18n::text(character_saves_
                   ? (saving ? N_("Select a character save to overwrite, or enter a new name.")
                      : N_("Select a character save. Previous versions are available for recovery."))
                   : (saving ? N_("Select a save to overwrite, or enter a new name.")
                      : N_("Select a save. Previous versions are available for recovery."))));
    required_node<LineEdit>(*this, "Name").set_placeholder(
        i18n::text(character_saves_ ? N_("Character save name") : N_("Save name")));
    auto *list = &required_node<ItemList>(*this, "Slots");
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
            auto name = decoded(base.stem().string());
            list->add_item(String::utf8(name.c_str()) +
                           (backup ? i18n::text(" (previous version)") : String()));
        }
        required_node<Label>(*this, "Status").set_text("");
    }
    catch (const std::exception &e)
    {
        required_node<Label>(*this, "Status").set_text(i18n::text(e.what()));
    }
    required_node<LineEdit>(*this, "Name").set_text("");
    required_node<LineEdit>(*this, "Name").set_visible(saving);
    required_node<Button>(*this, "Action").set_text(i18n::text(saving ? N_("Save") : N_("Load")));
    popup_centered();
    if (saving)
        required_node<LineEdit>(*this, "Name").grab_focus();
    else
        list->grab_focus();
}

void SaveSlots::changed(String)
{
    confirmed_ = false;
    pending_.clear();
    required_node<Button>(*this, "Action").set_text(i18n::text(saving_ ? N_("Save") : N_("Load")));
}

void SaveSlots::select(std::int64_t index)
{
    changed("");
    if (index < 0 || static_cast<std::size_t>(index) >= paths_.size())
        return;
    if (saving_)
        required_node<LineEdit>(*this, "Name").set_text(
            String::utf8(decoded(paths_[index].stem().string()).c_str()));
}

void SaveSlots::act()
{
    try
    {
        std::filesystem::path path;
        if (saving_)
        {
            auto text = required_node<LineEdit>(*this, "Name").get_text().strip_edges();
            std::string name = text.utf8().get_data();
            if (name.empty() || name.size() > 120)
                throw std::runtime_error("Enter a save name of at most 120 UTF-8 bytes.");
            path = directory_ / (encoded(name) + ".ogs");
        }
        else
        {
            auto selected = required_node<ItemList>(*this, "Slots").get_selected_items();
            if (selected.is_empty())
                throw std::runtime_error("Select a save first.");
            path = paths_.at(selected[0]);
        }
        if ((!saving_ || std::filesystem::exists(path)) && (!confirmed_ || pending_ != path))
        {
            confirmed_ = true;
            pending_ = path;
            required_node<Label>(*this, "Status").set_text(i18n::text(
                        saving_ ? N_("Overwrite this save? Its previous version will be retained.")
                        : character_saves_
                        ? N_("Load this character? Any unsaved demo progress will be discarded.")
                        : N_("Load this save? Any unsaved campaign progress will be discarded.")));
            required_node<Button>(*this, "Action").set_text(
                i18n::text(saving_ ? N_("Overwrite") : N_("Confirm load")));
            return;
        }
        if (saving_)
            save_(path);
        else
            load_(path);
        hide();
    }
    catch (const std::exception &e)
    {
        confirmed_ = false;
        required_node<Label>(*this, "Status").set_text(i18n::text(e.what()));
        required_node<Button>(*this, "Action")
        .set_text(i18n::text(saving_ ? N_("Save") : N_("Load")));
    }
}

void SaveSlots::close()
{
    hide();
}
