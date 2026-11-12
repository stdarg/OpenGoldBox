#include "godot_text.h"
#include "portrait_catalog.h"
#include "application_settings.h"
#include "game_resources.h"
#include "godot_images.h"
#include "godot_path.h"
#include "unloadable_art.h"
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
using presentation::gs;

std::string normalized(std::string s)
{
    for (auto &c : s)
    {
        if (c >= 'A' && c <= 'Z')
        {
            c += 32;
        }
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
    result.folder_ = new_art_path("portraits");
    if (result.folder_.is_empty())
    {
        return result;
    }
    Ref<JSON> json;
    json.instantiate();
    const auto catalog_file = result.folder_.path_join("portraits.json");
    if (json->parse(FileAccess::get_file_as_string(catalog_file)) != OK ||
        json->get_data().get_type() != Variant::DICTIONARY)
    {
        log_unloadable_art(std::string("Cannot load the portrait catalog ") +
                           catalog_file.utf8().get_data());
        return result;
    }
    const Dictionary catalog = json->get_data();
    const Array keys = catalog.keys();
    for (int64_t i = 0; i < keys.size(); ++i)
    {
        const auto filename = std::string(String(keys[i]).utf8().get_data());
        const Dictionary entry = catalog[keys[i]].get_type() == Variant::DICTIONARY
                                     ? Dictionary(catalog[keys[i]])
                                     : Dictionary();
        const auto field = [&](const char *key)
        {
            return entry.has(key) && entry[key].get_type() == Variant::STRING
                       ? std::string(String(entry[key]).utf8().get_data())
                       : std::string();
        };
        Portrait p{filename, field("Gender"), field("Class"), field("Race")};
        const auto file = result.file_path(filename);
        // A file name a save may hold: no folders, a .png.
        const bool storable = [&]
        {
            opengold::por::CharacterAppearance a;
            a.portrait = filename;
            try
            {
                opengold::por::validate_character_appearance(a);
                return true;
            }
            catch (const std::exception &)
            {
                return false;
            }
        }();
        if (!storable || p.gender.empty() || p.klass.empty() || p.race.empty())
        {
            log_unloadable_art("Cannot load portrait " + std::string(file.utf8().get_data()) +
                               ": its entry in portraits.json is not usable");
            continue;
        }
        if (!FileAccess::file_exists(file))
        {
            log_unloadable_art("Cannot load portrait " + std::string(file.utf8().get_data()) +
                               ": the file is missing");
            continue;
        }
        result.portraits_.push_back(std::move(p));
    }
    std::sort(result.portraits_.begin(), result.portraits_.end(),
              [](const auto &a, const auto &b)
              {
                  return a.filename < b.filename;
              });
    return result;
}

String PortraitCatalog::file_path(const std::string &filename) const
{
    return folder_.is_empty() ? String() : folder_.path_join(gs(filename));
}

bool PortraitCatalog::same(const std::string &catalog_value, const std::string &draft_value)
{
    return normalized(catalog_value) == normalized(draft_value);
}

std::string PortraitCatalog::recommended(const opengold::rules::CharacterDraft &draft) const
{
    if (portraits_.empty())
    {
        return {};
    }
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

std::string PortraitCatalog::of_race_and_gender(const opengold::rules::CharacterDraft &draft,
                                                std::uint32_t pick) const
{
    std::vector<const Portrait *> matches;
    for (const auto &p : portraits_)
    {
        if (normalized(p.race) == normalized(draft.race) &&
            normalized(p.gender) == normalized(draft.gender))
        {
            matches.push_back(&p);
        }
    }
    if (matches.empty())
    {
        return recommended(draft);
    }
    return matches[pick % matches.size()]->filename;
}

std::string PortraitCatalog::shown_file(const opengold::por::CharacterAppearance &appearance,
                                        const opengold::rules::CharacterDraft &draft) const
{
    const bool cataloged = std::any_of(portraits_.begin(), portraits_.end(),
                                       [&](const auto &p)
                                       {
                                           return p.filename == appearance.portrait;
                                       });
    return cataloged ? appearance.portrait : recommended(draft);
}

bool PortraitCatalog::shows_new(const opengold::por::CharacterAppearance &appearance,
                                const opengold::rules::CharacterDraft &draft)
{
    if (settings::original_portraits() || portraits_.empty())
    {
        return false;
    }
    return new_texture(shown_file(appearance, draft)).is_valid();
}

Ref<ImageTexture> PortraitCatalog::texture(const opengold::por::CharacterAppearance &appearance,
                                           const opengold::rules::CharacterDraft &draft)
{
    return shows_new(appearance, draft) ? new_texture(shown_file(appearance, draft))
                                        : original(appearance);
}

Ref<ImageTexture> PortraitCatalog::new_texture(const std::string &filename)
{
    if (const auto found = textures_.find(filename); found != textures_.end())
    {
        return found->second;
    }
    if (unloadable_.contains(filename))
    {
        return {};
    }
    const auto path = file_path(filename);
    const auto image = path.is_empty() ? Ref<Image>() : Image::load_from_file(path);
    if (image.is_null())
    {
        unloadable_.insert(filename);
        log_unloadable_art("Cannot load portrait " + std::string(path.utf8().get_data()));
        return {};
    }
    auto result = ImageTexture::create_from_image(image);
    // Named for its file, so a check can tell which portrait a page draws.
    result->set_name(gs(filename));
    textures_.emplace(filename, result);
    return result;
}

const opengold::por::CharacterArt &PortraitCatalog::original_art()
{
    if (!original_art_)
    {
        auto art =
            opengold::por::CharacterArt::load(presentation::path_from_godot(settings::game_path()));
        presentation::load_additional_portrait_heads(art);
        original_art_ = std::move(art);
    }
    return *original_art_;
}

Ref<ImageTexture> PortraitCatalog::original(const opengold::por::CharacterAppearance &appearance)
{
    const auto &art = original_art();
    // A saved part the game's files lack shows as their first.
    auto shown = appearance;
    if (!art.heads().contains(shown.portrait_head))
    {
        shown.portrait_head = art.heads().begin()->first;
    }
    if (!art.bodies().contains(shown.portrait_body))
    {
        shown.portrait_body = art.bodies().begin()->first;
    }
    const auto name = "original-head-" + std::to_string(shown.portrait_head) + "-body-" +
                      std::to_string(shown.portrait_body);
    if (const auto found = textures_.find(name); found != textures_.end())
    {
        return found->second;
    }
    auto result = presentation::image_texture(art.portrait(shown));
    result->set_name(gs(name));
    textures_.emplace(name, result);
    return result;
}
