#ifndef OPENGOLDBOX_PORTRAIT_CATALOG_H
#define OPENGOLDBOX_PORTRAIT_CATALOG_H
#include "opengold/character_art.h"
#include "opengold/character_rules.h"
#include <godot_cpp/classes/image_texture.hpp>
#include <cstdint>
#include <list>
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
    PortraitCatalog() = default;
    // Move-only: the kept textures point into the catalog's own use order.
    PortraitCatalog(const PortraitCatalog &) = delete;
    PortraitCatalog &operator=(const PortraitCatalog &) = delete;
    PortraitCatalog(PortraitCatalog &&) noexcept = default;
    PortraitCatalog &operator=(PortraitCatalog &&) noexcept = default;
    ~PortraitCatalog() = default;

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
    // The portrait the character shows. The most recently used textures are
    // kept, so a picture shown again is not read again.
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
    // A kept texture, now the most recently used; null when it is not kept.
    [[nodiscard]] godot::Ref<godot::ImageTexture> kept(const std::string &name);
    // Keeps a texture, forgetting the least recently used beyond the limit:
    // a new portrait is a large picture, and a player may look at hundreds.
    void keep(const std::string &name, const godot::Ref<godot::ImageTexture> &texture);

    struct KeptTexture
    {
        godot::Ref<godot::ImageTexture> texture;
        std::list<std::string>::iterator use;
    };

    godot::String folder_;
    std::vector<Portrait> portraits_;
    std::map<std::string, KeptTexture> textures_;
    // The kept textures' names, the most recently used first.
    std::list<std::string> recent_uses_;
    std::set<std::string> unloadable_;
    std::optional<opengold::por::CharacterArt> original_art_;
};
#endif
