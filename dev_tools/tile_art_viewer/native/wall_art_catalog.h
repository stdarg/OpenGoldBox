#ifndef OPENGOLD_TILE_ART_CATALOG_H
#define OPENGOLD_TILE_ART_CATALOG_H

#include "opengold/formats.h"
#include "opengold/geo_map.h"
#include "opengold/equipment.h"
#include "opengold/wall_art.h"
#include "opengold/wall_art_replacements.h"
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <array>
#include <map>
#include <string>
#include <vector>

class WallArtCatalog : public godot::Control
{
    GDCLASS(WallArtCatalog, godot::Control)

  public:
    godot::String load_directory(const godot::String &directory);
    godot::String load_shop_directory(const godot::String &directory);
    [[nodiscard]] godot::Array entries() const;
    [[nodiscard]] godot::Ref<godot::Image> image_at(int index) const;
    [[nodiscard]] godot::Array maps() const;
    [[nodiscard]] godot::Array shop_stock(int record) const;
    [[nodiscard]] godot::Array item_catalog() const;
    // Reads new wall art the way the game does (data/art/walls).
    godot::String load_new_art(const godot::String &folder);
    // Appearance key ("WALLDEF1.DAX:1:4") to its ten new pieces.
    [[nodiscard]] godot::Dictionary new_art_sets() const;
    [[nodiscard]] godot::Dictionary map_cell(int map_index, int profile_index, int x, int y,
                                             int view) const;
    // With new_art, views use the game's composition of new and original art;
    // pixels are in that image's coordinates.
    [[nodiscard]] godot::Ref<godot::Image> map_view(int map_index, int profile_index, int x, int y,
                                                    int facing, bool new_art) const;
    [[nodiscard]] godot::Dictionary map_pick(int map_index, int profile_index, int x, int y,
                                             int facing, int pixel_x, int pixel_y,
                                             bool new_art) const;
    [[nodiscard]] godot::Array map_visible_pieces(int map_index, int profile_index, int x, int y,
                                                  int facing, bool new_art) const;

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
        std::vector<std::string> appearance_keys;
        opengold::por::WallArtSet art;
        opengold::por::WallArtSet new_art;
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
    std::map<unsigned, std::vector<opengold::por::Equipment>> shop_stock_;
    godot::Array item_catalog_;
    opengold::por::WallArtReplacements new_art_;
};

#endif
