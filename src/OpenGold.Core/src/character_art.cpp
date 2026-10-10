#include "opengold/character_art.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>

namespace opengold::por
{
namespace
{
constexpr std::array<AdditionalPortraitHead, 10> additional_heads
{
    {   {256, "gnome-male.png", "Gnome / Male", "gnome", "male", 36, 52, 40},
        {257, "gnome-female.png", "Gnome / Female", "gnome", "female", 37, 49, 40},
        {258, "orc-male.png", "Orc / Male", "orc", "male", 35, 51, 40},
        {259, "orc-female.png", "Orc / Female", "orc", "female", 29, 50, 40},
        {260, "goliath-male.png", "Goliath / Male", "goliath", "male", 33, 52, 40},
        {261, "goliath-female.png", "Goliath / Female", "goliath", "female", 34, 50, 40},
        {262, "tiefling-male.png", "Tiefling / Male", "tiefling", "male", 35, 52, 40},
        {263, "tiefling-female.png", "Tiefling / Female", "tiefling", "female", 36, 49, 40},
        {264, "dragonborn-male.png", "Dragonborn / Male", "dragonborn", "male", 34, 54, 40},
        {265, "dragonborn-female.png", "Dragonborn / Female", "dragonborn", "female", 35, 51, 40}
    }};

const AdditionalPortraitHead &additional_head(unsigned id)
{
    const auto entry = std::find_if(additional_heads.begin(), additional_heads.end(),
                                    [&](const auto & head)
    {
        return head.id == id;
    });
    if (entry == additional_heads.end())
        throw std::runtime_error("Unknown additional portrait head");
    return *entry;
}

Image fit_neck(const Image &head, const Image &body, const AdditionalPortraitHead &placement)
{
    // Original HEAD bottoms normally occupy x=36..55. BODY openings vary:
    // inspect only the neck region's skin pixels, not armor, collars or hair.
    unsigned left = 88, right = 0;
    for (unsigned x = 30; x < 62; ++x)
    {
        const auto p = x * 4;
        if (body.rgba[p] == 255 && body.rgba[p + 1] == 85 && body.rgba[p + 2] == 85)
        {
            left = std::min(left, x);
            right = x + 1;
        }
    }
    if (right <= left || right - left < 10 || right - left > 28)
    {
        left = 36;
        right = 56;
    }
    const double source_center = (placement.neck_left + placement.neck_right) / 2.0;
    const double target_center = (left + right) / 2.0;
    const double neck_scale = double(right - left) / (placement.neck_right - placement.neck_left);
    Image result = head;
    for (unsigned y = 0; y < 40; ++y)
        for (unsigned x = 0; x < 88; ++x)
        {
            // Translate the face intact. Only the lowest five rows gradually widen
            // or narrow to the body's opening; never stretch the face or horns.
            const double blend = y <= 34 ? 0.0 : double(y - 34) / 5.0;
            const double scale = 1.0 + (neck_scale - 1.0) * blend;
            const int from_x =
                static_cast<int>(std::floor(source_center + (x + 0.5 - target_center) / scale));
            const auto to = (y * 88 + x) * 4;
            for (unsigned c = 0; c < 3; ++c)
                result.rgba[to + c] =
                    from_x >= 0 && from_x < 88 ? head.rgba[(y * 88 + from_x) * 4 + c] : 0;
            result.rgba[to + 3] = 255;
        }
    return result;
}

// 5/13 belong to the cap. The six user-customizable regions skip that pair.
constexpr std::array<int, 8> color_regions{-1, 1, 4, 5, 2, -1, 3, 0};

std::vector<std::uint8_t> composed_pixels(const IndexedIcon &head, const IndexedIcon &body)
{
    if (head.width != 24 || body.width != 24 || !head.height || head.height > 24 ||
            body.height != 24 || head.pixels.size() != head.width * head.height ||
            body.pixels.size() != 576)
        throw std::runtime_error("Invalid character icon components");
    auto pixels = body.pixels;
    for (unsigned p = 0; p < head.pixels.size(); ++p)
        if (head.pixels[p])
            pixels[p] = head.pixels[p];
    if (std::any_of(pixels.begin(), pixels.end(),
                    [](auto c)
{
    return c > 15;
}))
    throw std::runtime_error("Invalid character icon pixel");
    return pixels;
}

IndexedIcon with_dagger(IndexedIcon body, unsigned bank)
{
    const bool action = bank >= 128, tall = bank % 128 == 64;
    const unsigned tip_y = action ? (tall ? 17u : 18u) : (tall ? 4u : 6u);
    for (unsigned y = 0; y < 24; ++y)
        for (unsigned x = 0; x < 24; ++x)
        {
            auto &color = body.pixels[y * 24 + x];
            if (color != 7 && color != 15)
                continue;
            if ((action && y > tip_y) || (!action && y < tip_y))
                color = 0;
            else if (y == tip_y)
                color = x == (action ? 19u : 4u) ? 15 : 0;
        }
    return body;
}

IndexedIcon without_wand(IndexedIcon body, unsigned bank)
{
    // Body 21's wand is the gray projection from the hand. Keep the shield,
    // hand, and the rest of the original decoded pixels in all four banks.
    const auto erase = [&](unsigned y, unsigned first, unsigned last)
    {
        for (unsigned x = first; x <= last; ++x)
            if (body.pixels[y * 24 + x] == 8)
                body.pixels[y * 24 + x] = 0;
    };
    switch (bank)
    {
    case 0:
        erase(15, 14, 15);
        erase(16, 15, 17);
        erase(17, 17, 19);
        erase(18, 19, 20);
        break;
    case 64:
        erase(13, 14, 15);
        erase(14, 15, 17);
        erase(15, 17, 19);
        erase(16, 19, 20);
        break;
    case 128:
        erase(15, 15, 21);
        break;
    case 192:
        erase(13, 15, 21);
        break;
    default:
        throw std::runtime_error("Invalid combat body bank");
    }
    return body;
}
} // namespace

std::span<const AdditionalPortraitHead> additional_portrait_heads()
{
    return additional_heads;
}

std::optional<unsigned> matching_portrait_head(std::string_view race, std::string_view gender)
{
    for (const auto &head : additional_heads)
        if (head.race == race && head.gender == gender)
            return head.id;
    return std::nullopt;
}

Image prepare_portrait_head(const Image &source, unsigned head_id)
{
    const auto &placement = additional_head(head_id);
    if (!source.width || !source.height || source.width > 8192 || source.height > 8192 ||
            !consistent(source))
        throw std::runtime_error("Invalid portrait source image");
    unsigned bottom = source.height;
    const auto visible = [&](unsigned x, unsigned y)
    {
        const auto p = (std::size_t(y) * source.width + x) * 4;
        return source.rgba[p + 3] > 127 &&
               std::max({source.rgba[p], source.rgba[p + 1], source.rgba[p + 2]}) > 24;
    };
    while (bottom)
    {
        bool content = false;
        for (unsigned x = 0; x < source.width; ++x)
            if (visible(x, bottom - 1))
            {
                content = true;
                break;
            }
        if (content)
            break;
        --bottom;
    }
    if (!bottom)
        throw std::runtime_error("Portrait source has no visible head");
    Image result;
    result.width = 88;
    result.height = 40;
    result.rgba.resize(88 * 40 * 4);
    for (unsigned y = 0; y < 40; ++y)
        for (unsigned x = 0; x < 88; ++x)
        {
            const auto source_y = std::size_t(2 * y + 1) * bottom * placement.retained_rows / 3200;
            const auto from = (source_y * source.width + (2 * x + 1) * source.width / 176) * 4;
            const auto to = (y * 88 + x) * 4;
            // Composite alpha on the original portrait's black background.
            for (unsigned c = 0; c < 3; ++c)
                result.rgba[to + c] = unsigned(source.rgba[from + c]) * source.rgba[from + 3] / 255;
            result.rgba[to + 3] = 255;
        }
    return result;
}

bool CharacterColorUsage::contains(unsigned bank, unsigned part) const
{
    if (bank >= 2 || part >= 6)
        throw std::runtime_error("Invalid character color region");
    return ready[bank][part] || action[bank][part];
}

void validate_character_appearance(const CharacterAppearance &a)
{
    if ((a.portrait_head > 255 && std::none_of(additional_heads.begin(), additional_heads.end(),
            [&](const auto & h)
{
    return h.id == a.portrait_head;
})) ||
a.portrait_body > 255 || a.combat_head >= 14 || a.combat_body >= 35)
    throw std::runtime_error("Invalid appearance reference");
    if (!a.portrait.empty() &&
            (a.portrait.size() > 160 || !a.portrait.ends_with(".png") ||
             a.portrait.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789-_.") !=
             std::string::npos ||
             a.portrait.find("..") != std::string::npos))
        throw std::runtime_error("Invalid portrait filename");
    for (const auto &bank : a.colors)
        for (auto color : bank)
            (void)character_color(color);
}

std::array<std::uint8_t, 3> character_color(unsigned index)
{
    constexpr std::array<std::array<std::uint8_t, 3>, 16> palette{{{0, 0, 0},
            {0, 0, 170},
            {0, 170, 0},
            {0, 170, 170},
            {170, 0, 0},
            {170, 0, 170},
            {170, 85, 0},
            {170, 170, 170},
            {85, 85, 85},
            {85, 85, 255},
            {85, 255, 85},
            {85, 255, 255},
            {255, 85, 85},
            {255, 85, 255},
            {255, 255, 85},
            {255, 255, 255}
        }};
    if (index >= palette.size())
        throw std::runtime_error("Invalid character color");
    return palette[index];
}

IndexedIcon decode_character_icon(std::span<const std::uint8_t> record)
{
    if (record.size() < 17)
        throw std::runtime_error("Truncated character icon");
    const unsigned height = record[0] + 256u * record[1],
                   width = (record[2] + 256u * record[3]) * 8;
    if (width != 24 || !height || height > 24 || record[8] != 1 ||
            record.size() != 17 + width * height / 2)
        throw std::runtime_error("Unsupported character icon layout");
    IndexedIcon result{width, height, {}};
    result.pixels.reserve(width * height);
    for (unsigned p = 0; p < width * height; ++p)
        result.pixels.push_back(p % 2 ? record[17 + p / 2] & 15 : record[17 + p / 2] >> 4);
    return result;
}

Image compose_character_icon(const IndexedIcon &head, const IndexedIcon &body,
                             const CharacterAppearance &appearance)
{
    const auto pixels = composed_pixels(head, body);
    for (const auto &bank : appearance.colors)
        for (auto c : bank)
            (void)character_color(c);
    Image result;
    result.width = result.height = 24;
    result.rgba.resize(24 * 24 * 4);
    // Source indices 1/9 = body, 2/10 = arms, 3/11 = legs,
    // 4/12 = hair/face, 6/14 = shield, 7/15 = weapon.
    for (unsigned p = 0; p < 576; ++p)
    {
        const unsigned index = pixels[p];
        auto rgb = character_color(index == 8 ? 0 : index);
        if (const auto region = color_regions[index & 7]; region >= 0)
            rgb = character_color(appearance.colors[index >> 3][region]);
        std::copy(rgb.begin(), rgb.end(), result.rgba.begin() + p * 4);
        result.rgba[p * 4 + 3] = index ? 255 : 0;
    }
    return result;
}

CharacterArt CharacterArt::load(const std::filesystem::path &directory)
{
    std::map<std::string, std::filesystem::path> paths;
    for (const auto &entry : std::filesystem::directory_iterator(directory))
        if (entry.is_regular_file())
        {
            auto name = entry.path().filename().string();
            for (auto &c : name)
                if (c >= 'a' && c <= 'z')
                    c -= 32;
            if (!paths.emplace(name, entry.path()).second)
                throw std::runtime_error("Ambiguous art archive: " + name);
        }
    const auto read = [&](const std::string & name)
    {
        if (!paths.contains(name))
            throw std::runtime_error("Missing character art: " + name);
        const auto &path = paths.at(name);
        const auto size = std::filesystem::file_size(path);
        if (size > 32 * 1024 * 1024)
            throw std::runtime_error("Character art archive exceeds size limit");
        std::ifstream in(path, std::ios::binary);
        std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(in), {}};
        if (in.bad() || bytes.size() != size)
            throw std::runtime_error("Cannot read character art: " + name);
        auto decoded = decode_dax_archive(bytes);
        if (!decoded)
            throw std::runtime_error("Invalid character art archive: " + name);
        return decoded.records;
    };
    CharacterArt art;
    for (unsigned disk = 1; disk <= 8; ++disk)
        for (const auto &stem :
                {
                    std::string("HEAD"), std::string("BODY")
                })
        {
            const auto archive = stem + std::to_string(disk) + ".DAX";
            for (const auto &r : read(archive))
            {
                // Present the available original parts. Do not interpret a creator
                // menu index as an archive ID, or silently invent missing images.
                auto decoded = decode_ega_picture(r.bytes);
                if (!decoded || decoded.image.width != 88 ||
                        decoded.image.height != (stem == "HEAD" ? 40 : 48))
                    throw std::runtime_error("Unsupported player portrait layout");
                auto &parts = stem == "HEAD" ? art.heads_ : art.bodies_;
                if (parts.contains(r.id) && parts.at(r.id).image.rgba != decoded.image.rgba)
                    throw std::runtime_error("Conflicting player portrait ID across archives");
                parts.try_emplace(r.id, PortraitPart{archive, std::move(decoded.image)});
            }
        }
    for (const auto &r : read("CHEAD.DAX"))
        art.combat_heads_.emplace(r.id, decode_character_icon(r.bytes));
    for (const auto &r : read("CBODY.DAX"))
        art.combat_bodies_.emplace(r.id, decode_character_icon(r.bytes));
    for (unsigned bank :
            {
                0u, 64u, 128u, 192u
            })
    {
        art.combat_bodies_.emplace(bank + 32, without_wand(art.combat_bodies_.at(bank + 21), bank));
        art.combat_bodies_.emplace(bank + 33, with_dagger(art.combat_bodies_.at(bank + 7), bank));
        art.combat_bodies_.emplace(bank + 34, with_dagger(art.combat_bodies_.at(bank + 24), bank));
    }
    if (art.heads_.empty() || art.bodies_.empty())
        throw std::runtime_error("Missing portraits");
    for (unsigned size :
            {
                0u, 64u
            })
        for (unsigned pose :
                {
                    0u, 128u
                })
        {
            for (unsigned id = 0; id < 14; ++id)
                if (!art.combat_heads_.contains(size + pose + id))
                    throw std::runtime_error("Missing combat head pose");
            for (unsigned id = 0; id < 32; ++id)
                if (!art.combat_bodies_.contains(size + pose + id))
                    throw std::runtime_error("Missing combat body pose");
        }
    return art;
}

void CharacterArt::validate(const CharacterAppearance &a) const
{
    validate_character_appearance(a);
    if (!heads_.contains(a.portrait_head) || !bodies_.contains(a.portrait_body))
        throw std::runtime_error("Invalid character appearance selection");
}

void CharacterArt::add_portrait_head(unsigned id, Image image)
{
    const auto entry = std::find_if(additional_heads.begin(), additional_heads.end(),
                                    [&](const auto & head)
    {
        return head.id == id;
    });
    if (entry == additional_heads.end() || image.width != 88 || image.height != 40 ||
            !consistent(image))
        throw std::runtime_error("Invalid additional portrait head");
    if (!heads_
            .emplace(id, PortraitPart{std::string(entry->filename), std::move(image),
                                      std::string(entry->label)})
            .second)
        throw std::runtime_error("Duplicate portrait head ID");
}

void CharacterArt::set_original_head(unsigned id, PortraitPart part)
{
    if (part.image.width != 88 || part.image.height != 40 || !consistent(part.image))
        throw std::runtime_error("A portrait head is 88x40");
    heads_.insert_or_assign(id, std::move(part));
}

void CharacterArt::set_original_body(unsigned id, PortraitPart part)
{
    if (part.image.width != 88 || part.image.height != 48 || !consistent(part.image))
        throw std::runtime_error("A portrait body is 88x48");
    bodies_.insert_or_assign(id, std::move(part));
}

namespace
{
void require_combat_icon(const IndexedIcon &icon)
{
    if (icon.width != 24 || !icon.height || icon.height > 24 ||
            icon.pixels.size() != icon.width * icon.height)
        throw std::runtime_error("A combat icon is 24 pixels wide and at most 24 high");
}
} // namespace

void CharacterArt::set_combat_head(unsigned id, IndexedIcon icon)
{
    require_combat_icon(icon);
    combat_heads_.insert_or_assign(id, std::move(icon));
}

void CharacterArt::set_combat_body(unsigned id, IndexedIcon icon)
{
    require_combat_icon(icon);
    combat_bodies_.insert_or_assign(id, std::move(icon));
}

Image CharacterArt::portrait(const CharacterAppearance &a) const
{
    validate(a);
    Image result;
    result.width = result.height = 88;
    const auto &head = heads_.at(a.portrait_head).image;
    const auto &body = bodies_.at(a.portrait_body).image;
    result.rgba = a.portrait_head > 255
                  ? fit_neck(head, body, additional_head(a.portrait_head)).rgba
                  : head.rgba;
    result.rgba.insert(result.rgba.end(), body.rgba.begin(), body.rgba.end());
    return result;
}

Image CharacterArt::icon(const CharacterAppearance &a, IconPose pose) const
{
    validate_character_appearance(a);
    const unsigned bank = (a.tall ? 64u : 0u) + (pose == IconPose::action ? 128u : 0u);
    return compose_character_icon(combat_heads_.at(bank + a.combat_head),
                                  combat_bodies_.at(bank + a.combat_body), a);
}

CharacterColorUsage CharacterArt::color_usage(const CharacterAppearance &a) const
{
    validate_character_appearance(a);
    CharacterColorUsage usage;
    for (bool action :
            {
                false, true
            })
    {
        const unsigned bank = (a.tall ? 64u : 0u) + (action ? 128u : 0u);
        const auto pixels = composed_pixels(combat_heads_.at(bank + a.combat_head),
                                            combat_bodies_.at(bank + a.combat_body));
        auto &counts = action ? usage.action : usage.ready;
        for (auto pixel : pixels)
            if (const auto part = color_regions[pixel & 7]; part >= 0)
                ++counts[pixel >> 3][part];
    }
    return usage;
}
} // namespace opengold::por
