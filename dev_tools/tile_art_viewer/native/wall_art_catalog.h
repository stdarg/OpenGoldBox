#ifndef OPENGOLD_TILE_ART_CATALOG_H
#define OPENGOLD_TILE_ART_CATALOG_H

#include "opengold/formats.h"
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/variant/array.hpp>
#include <string>
#include <vector>

class WallArtCatalog : public godot::Control
{
    GDCLASS(WallArtCatalog, godot::Control)

  public:
    godot::String load_directory(const godot::String &directory);
    [[nodiscard]] godot::Array entries() const;
    [[nodiscard]] godot::Ref<godot::Image> image_at(int index) const;

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
    std::vector<Entry> entries_;
};

#endif
