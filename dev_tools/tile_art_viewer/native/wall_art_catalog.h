#ifndef OPENGOLD_TILE_ART_CATALOG_H
#define OPENGOLD_TILE_ART_CATALOG_H

#include "opengold/formats.h"
#include "opengold/geo_map.h"
#include "opengold/wall_art.h"
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <array>
#include <string>
#include <vector>

class WallArtCatalog : public godot::Control
{
    GDCLASS(WallArtCatalog, godot::Control)

  public:
    godot::String load_directory(const godot::String &directory);
    [[nodiscard]] godot::Array entries() const;
    [[nodiscard]] godot::Ref<godot::Image> image_at(int index) const;
    [[nodiscard]] godot::Array maps() const;
    [[nodiscard]] godot::Dictionary map_cell(int map_index, int profile_index, int x, int y,
                                             int view) const;
    [[nodiscard]] godot::Ref<godot::Image> map_view(int map_index, int profile_index, int x, int y,
                                                    int facing) const;
    [[nodiscard]] godot::Dictionary map_pick(int map_index, int profile_index, int x, int y,
                                             int facing, int pixel_x, int pixel_y) const;
    [[nodiscard]] godot::Array map_visible_pieces(int map_index, int profile_index, int x, int y,
                                                  int facing) const;

  protected:
    static void _bind_methods();

  private:
    struct Entry
    {
        std::string archive;
        unsigned record{};
        unsigned appearance{};
        unsigned view{};
        opengold::Image image;
    };

    struct Profile
    {
        std::string label;
        std::string source;
        std::vector<std::array<int, 10>> appearance_entries;
        opengold::por::WallArtSet art;
    };

    struct MapEntry
    {
        std::string archive;
        unsigned record{};
        std::string name;
        opengold::por::GeoMap geometry;
        std::vector<Profile> profiles;
    };

    std::vector<Entry> entries_;
    std::vector<MapEntry> maps_;
};

#endif
