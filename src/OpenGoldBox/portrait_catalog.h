#ifndef OPENGOLDBOX_PORTRAIT_CATALOG_H
#define OPENGOLDBOX_PORTRAIT_CATALOG_H
#include "opengold/character_art.h"
#include "opengold/character_rules.h"
#include <godot_cpp/classes/image_texture.hpp>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

// The new art's portraits (portraits/ in its folder, listed in portraits.json)
// with the gender, class and race each shows, and the original game's head-and-body portraits that
// show in their place when the Options screen's Portraits are Original or a new portrait cannot be
// loaded. Kept apart from the character creation view so the pool and the party panel can show
// portraits without depending on the view (Effective C++ Item 31).
class PortraitCatalog
{
  public:
    struct Portrait
    {
        std::string filename, gender, klass, race;
    };

    // Reads the new art's portraits.json; none without the new art. A catalog
    // that cannot be read, or a portrait whose file is missing, is logged and
    // left out: the original portraits show.
    [[nodiscard]] static PortraitCatalog load();

    // Sorted by file name; empty when no new portrait could be read.
    [[nodiscard]] const std::vector<Portrait> &entries() const
    {
        return portraits_;
    }

    // Whether a catalog value names the same race, gender or class as a
    // draft's id, ignoring case, spaces, hyphens and underscores.
    [[nodiscard]] static bool same(const std::string &catalog_value,
                                   const std::string &draft_value);
    // The portrait best matching the draft's race, then gender, then class;
    // empty without new portraits.
    [[nodiscard]] std::string recommended(const opengold::rules::CharacterDraft &draft) const;
    // One of the portraits of the draft's race and gender, the `pick`th modulo
    // how many there are; the recommended portrait when there are none.
    [[nodiscard]] std::string of_race_and_gender(const opengold::rules::CharacterDraft &draft,
                                                 std::uint32_t pick) const;
    // Whether the character shows its new portrait (the appearance's, or the
    // recommended one when the catalog lacks it): the Options screen's
    // Portraits are New and its file loads. Otherwise it shows its original
    // head-and-body portrait.
    [[nodiscard]] bool shows_new(const opengold::por::CharacterAppearance &appearance,
                                 const opengold::rules::CharacterDraft &draft);
    // Where a new portrait's file is: an absolute path in the new art's
    // folder, or empty without the new art.
    [[nodiscard]] godot::String file_path(const std::string &filename) const;
    // The new portrait's file: the appearance's, or the recommended one when
    // the catalog lacks it.
    [[nodiscard]] std::string shown_file(const opengold::por::CharacterAppearance &appearance,
                                         const opengold::rules::CharacterDraft &draft) const;
    // The portrait the character shows. Each texture is loaded once and kept.
    [[nodiscard]] godot::Ref<godot::ImageTexture>
    texture(const opengold::por::CharacterAppearance &appearance,
            const opengold::rules::CharacterDraft &draft);
    // The character's original head-and-body portrait.
    [[nodiscard]] godot::Ref<godot::ImageTexture>
    original(const opengold::por::CharacterAppearance &appearance);
    // The original game's portrait heads and bodies, with the heads of races
    // the original lacks; read from the game's files when first needed.
    [[nodiscard]] const opengold::por::CharacterArt &original_art();

  private:
    // The new portrait's texture; null, and logged once, when it cannot load.
    [[nodiscard]] godot::Ref<godot::ImageTexture> new_texture(const std::string &filename);

    godot::String folder_;
    std::vector<Portrait> portraits_;
    std::map<std::string, godot::Ref<godot::ImageTexture>> textures_;
    std::set<std::string> unloadable_;
    std::optional<opengold::por::CharacterArt> original_art_;
};
#endif
