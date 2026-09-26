#include "character_creation_view.h"
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/classes/option_button.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <algorithm>
#include <set>
#include <stdexcept>
using namespace godot;
namespace
{
String gs(std::string_view s)
{
    return String::utf8(s.data(), s.size());
}
std::string normalized(std::string s)
{
    for (auto &c : s)
    {
        if (c >= 'A' && c <= 'Z')
            c += 32;
    }
    s.erase(std::remove_if(s.begin(), s.end(),
                           [](char c)
                           {
                               return c == '-' || c == '_' || c == ' ';
                           }),
            s.end());
    return s;
}
} // namespace
void CharacterCreationView::load_portraits()
{
    Ref<JSON> json;
    json.instantiate();
    if (json->parse(FileAccess::get_file_as_string("res://bin/portraits/portraits.json")) != OK ||
        json->get_data().get_type() != Variant::DICTIONARY)
        throw std::runtime_error("Cannot read portrait catalog. Run demos/build-rolf.cmd.");
    const Dictionary catalog = json->get_data();
    const Array keys = catalog.keys();
    for (int64_t i = 0; i < keys.size(); ++i)
    {
        if (catalog[keys[i]].get_type() != Variant::DICTIONARY)
            throw std::runtime_error("Invalid portrait metadata");
        const Dictionary entry = catalog[keys[i]];
        const auto field = [&](const char *key)
        {
            if (!entry.has(key) || entry[key].get_type() != Variant::STRING ||
                String(entry[key]).is_empty())
                throw std::runtime_error("Missing portrait metadata");
            return std::string(String(entry[key]).utf8().get_data());
        };
        Portrait p{String(keys[i]).utf8().get_data(), field("Gender"), field("Class"),
                   field("Race")};
        opengold::por::CharacterAppearance a;
        a.portrait = p.filename;
        opengold::por::validate_character_appearance(a);
        if (!ResourceLoader::get_singleton()->exists(gs("res://bin/portraits/" + p.filename)))
            throw std::runtime_error("Missing portrait: " + p.filename);
        portraits_.push_back(std::move(p));
    }
    if (portraits_.empty())
        throw std::runtime_error("Empty portrait catalog");
    std::sort(portraits_.begin(), portraits_.end(),
              [](const auto &a, const auto &b)
              {
                  return a.filename < b.filename;
              });
    for (int field = 0; field < 3; ++field)
    {
        const char *names[]{"PortraitGender", "PortraitClass", "PortraitRace"};
        const char *labels[]{"All genders", "All classes", "All races"};
        auto *control = get_node<OptionButton>(names[field]);
        control->add_item(labels[field]);
        std::set<std::string> values;
        for (const auto &p : portraits_)
            values.insert(field == 0 ? p.gender : field == 1 ? p.klass : p.race);
        for (const auto &value : values)
            control->add_item(gs(value));
        control->connect("item_selected",
                         callable_mp(this, &CharacterCreationView::portrait_filter_selected));
    }
}
std::string
CharacterCreationView::recommended_portrait(const opengold::rules::CharacterDraft &draft) const
{
    const Portrait *best = &portraits_.front();
    int score = -1;
    for (const auto &p : portraits_)
    {
        const int value = 4 * (normalized(p.race) == normalized(draft.race)) +
                          2 * (normalized(p.gender) == normalized(draft.gender)) +
                          (normalized(p.klass) == normalized(draft.character_class));
        if (value > score)
        {
            score = value;
            best = &p;
        }
    }
    return best->filename;
}
Ref<ImageTexture>
CharacterCreationView::portrait_texture(const opengold::por::CharacterAppearance &appearance,
                                        const opengold::rules::CharacterDraft &draft)
{
    auto filename = appearance.portrait;
    if (std::none_of(portraits_.begin(), portraits_.end(),
                     [&](const auto &p)
                     {
                         return p.filename == filename;
                     }))
        filename = recommended_portrait(draft);
    if (const auto found = portrait_textures_.find(filename); found != portrait_textures_.end())
        return found->second;
    Ref<Texture2D> texture =
        ResourceLoader::get_singleton()->load(gs("res://bin/portraits/" + filename));
    if (texture.is_null())
        throw std::runtime_error("Cannot load portrait: " + filename);
    auto result = ImageTexture::create_from_image(texture->get_image());
    portrait_textures_.emplace(filename, result);
    return result;
}
void CharacterCreationView::refresh_portraits()
{
    filtered_portraits_.clear();
    auto *list = get_node<OptionButton>("PortraitSelect");
    list->clear();
    const auto matches = [&](const char *node, const std::string &value)
    {
        auto *c = get_node<OptionButton>(node);
        return c->get_selected() <= 0 || c->get_item_text(c->get_selected()) == gs(value);
    };
    int selected = -1;
    for (std::size_t i = 0; i < portraits_.size(); ++i)
    {
        const auto &p = portraits_[i];
        if (!matches("PortraitGender", p.gender) || !matches("PortraitClass", p.klass) ||
            !matches("PortraitRace", p.race))
            continue;
        if (p.filename == creator_->appearance().portrait)
            selected = filtered_portraits_.size();
        filtered_portraits_.push_back(i);
        list->add_item(gs(p.klass + " / " + p.race + " / " + p.gender));
    }
    list->select(selected);
    if (selected < 0)
        list->set_text(filtered_portraits_.empty() ? "No matching portraits" : "Choose portrait");
    const bool disabled = added_to_party_ || filtered_portraits_.empty();
    list->set_disabled(disabled);
    get_node<Button>("PortraitPrevious")->set_disabled(disabled);
    get_node<Button>("PortraitNext")->set_disabled(disabled);
    const auto &filename = creator_->appearance().portrait;
    list->set_tooltip_text(gs("Current portrait: " + filename + "\n" +
                              std::to_string(filtered_portraits_.size()) +
                              " matching portraits. Filters do not change your character."));
}
void CharacterCreationView::portrait_filter_selected(std::int64_t)
{
    if (!refreshing_)
        refresh();
}
void CharacterCreationView::portrait_part(int direction)
{
    if (added_to_party_ || filtered_portraits_.empty())
        return;
    const int current = get_node<OptionButton>("PortraitSelect")->get_selected(),
              count = filtered_portraits_.size();
    portrait_selected(current < 0 ? (direction > 0 ? 0 : count - 1)
                                  : (current + direction + count) % count);
}
void CharacterCreationView::portrait_selected(std::int64_t index)
{
    if (refreshing_ || added_to_party_)
        return;
    perform(
        [&]
        {
            if (index < 0 || static_cast<std::size_t>(index) >= filtered_portraits_.size())
                throw std::runtime_error("Invalid portrait selection");
            auto a = creator_->appearance();
            a.portrait = portraits_.at(filtered_portraits_[index]).filename;
            creator_->appearance(a);
            if (completed_)
                completed_->appearance(a);
            portrait_chosen_ = true;
        });
}
