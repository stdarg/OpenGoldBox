#include "portrait_catalog.h"
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <algorithm>
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

PortraitCatalog PortraitCatalog::load()
{
    PortraitCatalog result;
    Ref<JSON> json;
    json.instantiate();
    if (json->parse(FileAccess::get_file_as_string("res://bin/portraits/portraits.json")) != OK ||
            json->get_data().get_type() != Variant::DICTIONARY)
        throw std::runtime_error("Cannot read portrait catalog. Run build-opengoldbox.cmd.");
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
        result.portraits_.push_back(std::move(p));
    }
    if (result.portraits_.empty())
        throw std::runtime_error("Empty portrait catalog");
    std::sort(result.portraits_.begin(), result.portraits_.end(),
              [](const auto & a, const auto & b)
    {
        return a.filename < b.filename;
    });
    return result;
}

std::string PortraitCatalog::recommended(const opengold::rules::CharacterDraft &draft) const
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

Ref<ImageTexture> PortraitCatalog::texture(const opengold::por::CharacterAppearance &appearance,
        const opengold::rules::CharacterDraft &draft)
{
    auto filename = appearance.portrait;
    if (std::none_of(portraits_.begin(), portraits_.end(),
                     [&](const auto & p)
{
    return p.filename == filename;
}))
    filename = recommended(draft);
    if (const auto found = textures_.find(filename); found != textures_.end())
        return found->second;
    Ref<Texture2D> texture =
        ResourceLoader::get_singleton()->load(gs("res://bin/portraits/" + filename));
    if (texture.is_null())
        throw std::runtime_error("Cannot load portrait: " + filename);
    auto result = ImageTexture::create_from_image(texture->get_image());
    textures_.emplace(filename, result);
    return result;
}
