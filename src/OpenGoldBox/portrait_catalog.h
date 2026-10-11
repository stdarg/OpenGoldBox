#ifndef OPENGOLDBOX_PORTRAIT_CATALOG_H
#define OPENGOLDBOX_PORTRAIT_CATALOG_H
#include "opengold/character_art.h"
#include "opengold/character_rules.h"
#include <godot_cpp/classes/image_texture.hpp>
#include <map>
#include <string>
#include <vector>

// The portraits shipped in res://bin/portraits with the gender, class and race
// each shows. Kept apart from the character creation view so the pool and the
// party panel can show portraits without depending on the view (Effective C++
// Item 31).
class PortraitCatalog
{
  public:
    struct Portrait
    {
        std::string filename, gender, klass, race;
    };

    // Reads portraits.json; throws if it is unreadable, empty or names a
    // portrait file that is missing.
    [[nodiscard]] static PortraitCatalog load();

    // Sorted by file name; never empty.
    [[nodiscard]] const std::vector<Portrait> &entries() const
    {
        return portraits_;
    }

    // The portrait best matching the draft's race, then gender, then class.
    [[nodiscard]] std::string recommended(const opengold::rules::CharacterDraft &draft) const;
    // The appearance's portrait, or the recommended one when the catalog lacks
    // it. Each texture is loaded once and kept.
    [[nodiscard]] godot::Ref<godot::ImageTexture>
    texture(const opengold::por::CharacterAppearance &appearance,
            const opengold::rules::CharacterDraft &draft);

  private:
    std::vector<Portrait> portraits_;
    std::map<std::string, godot::Ref<godot::ImageTexture>> textures_;
};
#endif
