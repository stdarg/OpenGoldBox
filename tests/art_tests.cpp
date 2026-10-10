#include "opengold/character_art.h"
#include "support/synthetic_dax.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace opengold;
using namespace opengold::por;
using namespace opengold::test;

namespace
{
using Bytes = std::vector<std::uint8_t>;

void check(bool condition, const char *message)
{
    if (!condition)
        throw std::logic_error(message);
}

template <class F> void rejects(F &&operation, std::string_view diagnostic)
{
    try
    {
        operation();
    }
    catch (const std::runtime_error &error)
    {
        check(std::string_view(error.what()).find(diagnostic) != std::string_view::npos,
              "Unexpected art rejection diagnostic");
        return;
    }
    throw std::logic_error("Invalid art was accepted");
}

Bytes picture(unsigned width, unsigned height, unsigned offset = 0)
{
    Bytes bytes(17 + width * height / 2);
    word(bytes, 0, height);
    word(bytes, 2, width / 8);
    bytes[8] = 1;
    for (unsigned p = 0; p < width * height; p += 2)
    {
        const auto color = [&](unsigned pixel)
        {
            // Transparent holes make the tests exercise both body visibility
            // and head occlusion. Different offsets distinguish sizes/poses.
            return pixel % 3 == 0 ? 0u : (pixel + offset) % 16;
        };
        bytes[17 + p / 2] = (color(p) << 4) | color(p + 1);
    }
    return bytes;
}

std::vector<DaxRecord> components(unsigned count, unsigned height)
{
    std::vector<DaxRecord> records;
    for (unsigned size :
            {
                0u, 64u
            })
        for (unsigned pose :
                {
                    0u, 128u
                })
            for (unsigned id = 0; id < count; ++id)
                records.push_back({static_cast<std::uint8_t>(size + pose + id),
                                   picture(24, height, id + size / 64 + pose / 32)});
    return records;
}

class Fixture
{
  public:
    Fixture()
        : path_(std::filesystem::temp_directory_path() /
                ("opengold-art-" +
                 std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())))
    {
        check(std::filesystem::create_directory(path_), "Unique art fixture directory");
    }

    ~Fixture()
    {
        std::error_code ignored;
        std::filesystem::remove_all(path_, ignored);
    }

    Fixture(const Fixture &) = delete;
    Fixture &operator=(const Fixture &) = delete;

    const std::filesystem::path &path() const
    {
        return path_;
    }

    void write(std::string_view name, const Bytes &bytes) const
    {
        std::ofstream output(path_ / name, std::ios::binary);
        output.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
        if (!output)
            throw std::runtime_error("Cannot write art fixture");
    }

    void populate() const
    {
        for (unsigned disk = 1; disk <= 8; ++disk)
        {
            write("HEAD" + std::to_string(disk) + ".DAX", literal_dax({{1, picture(88, 40)}}));
            write("BODY" + std::to_string(disk) + ".DAX", literal_dax({{1, picture(88, 48)}}));
        }
        write("CHEAD.DAX", literal_dax(components(14, 8)));
        write("CBODY.DAX", literal_dax(components(32, 24)));
    }

  private:
    std::filesystem::path path_;
};

CharacterArt archive_tests()
{
    Fixture fixture;
    rejects(
        [&]
    {
        (void)CharacterArt::load(fixture.path());
    },
    "Missing character art");
    fixture.populate();
    auto art = CharacterArt::load(fixture.path());
    check(art.heads().size() == 1 && art.bodies().size() == 1,
          "Identical portrait IDs across disks merge");
    check(art.combat_heads().size() == 56 && art.combat_bodies().size() == 140,
          "Original and derived bodies load in all sizes and poses");
    for (unsigned bank :
            {
                0u, 64u, 128u, 192u
            })
        for (unsigned id :
                {
                    33u, 34u
                })
        {
            const auto source_id = id == 33 ? 7u : 24u;
            const auto original = decode_character_icon(
                                      picture(24, 24, source_id + (bank % 128) / 64 + (bank / 128) * 4));
            check(art.combat_bodies().at(bank + source_id).pixels == original.pixels,
                  "Dagger derivation leaves source sword intact");
            const auto &dagger = art.combat_bodies().at(bank + id).pixels;
            unsigned changes = 0;
            for (unsigned p = 0; p < 576; ++p)
                if (dagger[p] != original.pixels[p])
                {
                    check(
                        (original.pixels[p] == 7 || original.pixels[p] == 15) &&
                        (dagger[p] == 0 || dagger[p] == 15),
                        "Dagger derivation only changes blade pixels, preserving hands, shield and body");
                    ++changes;
                }
            check(changes > 0, "Both dagger bodies shorten the blade in every size and pose");
        }
    std::filesystem::rename(fixture.path() / "CHEAD.DAX", fixture.path() / "chead.dax");
    check(CharacterArt::load(fixture.path()).combat_heads().size() == 56,
          "DOS filenames are case independent");
    std::filesystem::rename(fixture.path() / "chead.dax", fixture.path() / "CHEAD.DAX");

    const auto invalid_archive =
        [&](const char *filename, const Bytes & replacement, const char *message)
    {
        fixture.write(filename, replacement);
        rejects(
            [&]
        {
            (void)CharacterArt::load(fixture.path());
        },
        message);
        fixture.populate();
    };
    invalid_archive("HEAD8.DAX", literal_dax({{1, picture(88, 40, 1)}}),
    "Conflicting player portrait ID");
    invalid_archive("HEAD8.DAX", literal_dax({{2, picture(24, 8)}}),
    "Unsupported player portrait layout");
    invalid_archive("HEAD8.DAX", Bytes{9, 0}, "Invalid character art archive");
    auto heads = components(14, 8);
    heads.pop_back();
    invalid_archive("CHEAD.DAX", literal_dax(heads), "Missing combat head pose");
    auto bodies = components(32, 24);
    bodies.erase(bodies.begin());
    invalid_archive("CBODY.DAX", literal_dax(bodies), "Missing combat body pose");
    invalid_archive("CBODY.DAX", literal_dax({{0, Bytes(16)}}), "Truncated character icon");
    for (unsigned disk = 1; disk <= 8; ++disk)
        fixture.write("HEAD" + std::to_string(disk) + ".DAX", literal_dax({}));
    rejects(
        [&]
    {
        (void)CharacterArt::load(fixture.path());
    },
    "Missing portraits");
    fixture.populate();
    std::filesystem::resize_file(fixture.path() / "HEAD1.DAX", 32 * 1024 * 1024 + 1);
    rejects(
        [&]
    {
        (void)CharacterArt::load(fixture.path());
    },
    "exceeds size limit");
    return art; // Its files disappear here; decoded art must own its data.
}

void composition_tests(const CharacterArt &art)
{
    CharacterAppearance appearance;
    check(art.portrait(appearance).rgba.size() == 88 * 88 * 4,
          "Portrait survives fixture destruction");
    for (bool tall :
            {
                false, true
            })
        for (unsigned head = 0; head < 14; ++head)
            for (unsigned body = 0; body < 35; ++body)
                for (bool action :
                        {
                            false, true
                        })
                {
                    appearance.tall = tall;
                    appearance.combat_head = head;
                    appearance.combat_body = body;
                    const auto icon = art.icon(appearance, action);
                    check(icon.width == 24 && icon.height == 24 && icon.rgba.size() == 576 * 4,
                          "Every synthetic head/body/size/pose composes");
                }

    constexpr std::array<unsigned, 6> masks{7, 1, 4, 6, 2, 3};
    for (bool tall :
            {
                false, true
            })
    {
        appearance = {};
        appearance.tall = tall;
        const auto usage = art.color_usage(appearance);
        for (bool action :
                {
                    false, true
                })
        {
            const unsigned source_bank = (tall ? 64 : 0) + (action ? 128 : 0);
            const auto &head = art.combat_heads().at(source_bank);
            const auto &body = art.combat_bodies().at(source_bank);
            const auto before = art.icon(appearance, action);
            for (unsigned bank = 0; bank < 2; ++bank)
                for (unsigned part = 0; part < 6; ++part)
                {
                    unsigned targeted = 0;
                    for (unsigned color = 0; color < 16; ++color)
                    {
                        auto changed = appearance;
                        changed.colors[bank][part] = color;
                        const auto after = art.icon(changed, action);
                        targeted = 0;
                        for (unsigned p = 0; p < 576; ++p)
                        {
                            const auto source = p < head.pixels.size() && head.pixels[p]
                                                ? head.pixels[p]
                                                : body.pixels[p];
                            const bool selected = source == masks[part] + 8 * bank;
                            const bool differs = !std::equal(before.rgba.begin() + 4 * p,
                                                             before.rgba.begin() + 4 * p + 4,
                                                             after.rgba.begin() + 4 * p);
                            check(
                                differs == (selected && color != appearance.colors[bank][part]),
                                "Recolor changes exactly the visible selected region, preserving transparency and fixed colors");
                            targeted += selected;
                        }
                    }
                    check(targeted == (action ? usage.action : usage.ready)[bank][part],
                          "Usage count equals the pixels recolored in each pose");
                    check(usage.contains(bank, part) ==
                          (usage.ready[bank][part] + usage.action[bank][part] > 0),
                          "A control is relevant if either pose uses its region");
                }
            check(art.icon(appearance, action).rgba == before.rgba,
                  "Recoloring never mutates cached components");
        }
    }
    rejects(
        [&]
    {
        (void)art.color_usage(appearance).contains(2, 0);
    },
    "Invalid character color region");
    rejects(
        [&]
    {
        (void)art.color_usage(appearance).contains(0, 6);
    },
    "Invalid character color region");
    // Combat icons never draw the portrait head. A newer race's head is only
    // registered where portraits are shown, so combat must not require it.
    appearance.portrait_head = 260;
    check(art.icon(appearance, false).width == 24 &&
          art.equipped_icon(appearance, 0, true).width == 24 &&
          art.combat_anatomy(appearance, false).width == 24,
          "Combat icons do not require the portrait head to be loaded");
    (void)art.color_usage(appearance);
    rejects(
        [&]
    {
        (void)art.portrait(appearance);
    },
    "Invalid character appearance selection");
    appearance.portrait_head = 2;
    rejects(
        [&]
    {
        (void)art.portrait(appearance);
    },
    "Invalid character appearance selection");
    auto head = art.combat_heads().at(0), body = art.combat_bodies().at(0);
    head.pixels[0] = 16;
    rejects(
        [&]
    {
        (void)compose_character_icon(head, body, {});
    },
    "Invalid character icon pixel");
    head.pixels.pop_back();
    rejects(
        [&]
    {
        (void)compose_character_icon(head, body, {});
    },
    "Invalid character icon components");
}
} // namespace

// The art's parts are private, so a part of the wrong size cannot be stored
// where composition would read past it (Effective C++ Item 22).
void parts_keep_their_sizes()
{
    CharacterArt art;
    const auto image = [](std::uint16_t width, std::uint16_t height)
    {
        return Image{.width = width, .height = height,
                     .rgba = std::vector<std::uint8_t>(std::size_t{width} * height * 4)};
    };
    art.set_original_head(1, PortraitPart{"fixture", image(88, 40), "Head"});
    art.set_original_body(1, PortraitPart{"fixture", image(88, 48), "Body"});
    check(art.heads().size() == 1 && art.bodies().size() == 1, "Correctly sized parts are kept");
    rejects([&]
    {
        art.set_original_head(2, PortraitPart{"fixture", image(88, 88), "Too tall"});
    }, "portrait head is 88x40");
    rejects([&]
    {
        art.set_original_body(2, PortraitPart{"fixture", image(88, 40), "Too short"});
    }, "portrait body is 88x48");
    rejects([&]
    {
        art.set_combat_body(0, IndexedIcon{24, 24, std::vector<std::uint8_t>(10)});
    }, "combat icon");
    check(art.heads().size() == 1 && art.bodies().size() == 1 && art.combat_bodies().empty(),
          "Refused parts are not stored");
}

int main()
{
    try
    {
        parts_keep_their_sizes();
        auto art = archive_tests();
        composition_tests(art);
        // Distinct authored anatomy makes whole-body substitution observable.
        // Equipment/body colors deliberately differ even where silhouettes overlap.
        for (unsigned bank :
                {
                    0u, 64u, 128u, 192u
                })
        {
            for (unsigned id = 0; id < 35; ++id)
            {
                auto blank = art.combat_bodies().at(bank + id);
                blank.pixels.assign(576, 0);
                art.set_combat_body(bank + id, std::move(blank));
            }
            auto head = art.combat_heads().at(bank);
            head.pixels.assign(head.pixels.size(), 0);
            head.pixels[0] = 12;
            art.set_combat_head(bank, std::move(head));
            auto base = art.combat_bodies().at(bank + 24);
            base.pixels[10 * 24 + 10] = 1;
            base.pixels[20 * 24 + 10] = 3;
            base.pixels[22 * 24 + 10] = 8;
            art.set_combat_body(bank + 24, std::move(base));
            for (unsigned donor :
                    {
                        6u, 28u
                    })
            {
                auto body = art.combat_bodies().at(bank + donor);
                auto &pixels = body.pixels;
                pixels[10 * 24 + 10] = 9;
                pixels[20 * 24 + 10] = 11; // Must never replace saved clothing.
                pixels[10 * 24 + 5] = donor == 6 ? 2 : 10;
                pixels[4 * 24 + 5] = donor == 6 ? 7 : 15;
                art.set_combat_body(bank + donor, std::move(body));
            }
        }
        for (bool tall :
                {
                    false, true
                })
            for (bool action :
                    {
                        false, true
                    })
            {
                CharacterAppearance a;
                a.tall = tall;
                a.combat_body = 24;
                const auto before = art.icon(a, action);
                const auto axe = art.equipped_icon(a, 6, action),
                           staff = art.equipped_icon(a, 28, action);
                const auto pixel_equal = [](const Image & first, const Image & second, unsigned p)
                {
                    return std::equal(first.rgba.begin() + 4 * p, first.rgba.begin() + 4 * p + 4,
                                      second.rgba.begin() + 4 * p);
                };
                for (const auto p :
                        {
                            0u, 250u, 490u, 538u
                        })
                    check(
                        pixel_equal(before, axe, p) && pixel_equal(before, staff, p),
                        "Equipment preserves saved head, torso, legs and boots in both sizes and poses");
                check(!pixel_equal(axe, staff, 245) && !pixel_equal(axe, staff, 101),
                      "Only wielding arms and equipment change with the donor");
                check(art.icon(a, action).rgba == before.rgba,
                      "Layered rendering never mutates original decoded art");
            }
        std::cout << "Synthetic art loading and recolor properties passed\n";
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
