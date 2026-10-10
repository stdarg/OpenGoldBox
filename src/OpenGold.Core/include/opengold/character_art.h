#ifndef OPENGOLD_CHARACTER_ART_H
#define OPENGOLD_CHARACTER_ART_H

#include "opengold/formats.h"
#include <array>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace opengold::por
{
// Art references and palette choices belong to presentation, independently of
// the selected rules edition. Head IDs 0..255 address original archives;
// additional heads have stable IDs above that range.
struct CharacterAppearance
{
    unsigned portrait_head{1}, portrait_body{1};
    unsigned combat_head{}, combat_body{};
    bool tall{true};
    // Weapon, body, hair/face, shield, arms, legs; Color-1 then Color-2.
    std::array<std::array<unsigned, 6>, 2> colors{{{7, 1, 6, 6, 1, 6}, {15, 9, 12, 14, 9, 14}}};
    std::string portrait; // Complete portrait basename; empty in legacy saves.
    bool operator==(const CharacterAppearance &) const = default;
};

// Structural validation; CharacterArt additionally checks available portrait IDs.
void validate_character_appearance(const CharacterAppearance &);

struct PortraitPart
{
    std::string archive;
    Image image;
    std::string label;
};

struct AdditionalPortraitHead
{
    unsigned id;
    std::string_view filename, label, race, gender;
    // Neck edges [left,right) and retained rows on the source's fitted 88x40
    // grid. Exclude dangling hair and rounded bottom remnants from the join.
    unsigned neck_left, neck_right, retained_rows;
};

[[nodiscard]] std::span<const AdditionalPortraitHead> additional_portrait_heads();
[[nodiscard]] std::optional<unsigned> matching_portrait_head(std::string_view race,
        std::string_view gender);
// Fit approved source artwork to the original head panel without palette
// quantization. Trim padding and crop at the cataloged neck baseline. The final
// neck placement is fitted to the selected original body during composition.
[[nodiscard]] Image prepare_portrait_head(const Image &source, unsigned head_id);

// Which of a combatant's two sprites: standing ready, or striking. A named
// pose rather than a bool, so the two cannot be swapped at a call site
// (Effective C++ Item 18).
enum class IconPose
{
    ready,
    action
};

struct IndexedIcon
{
    unsigned width{}, height{};
    std::vector<std::uint8_t> pixels;
};

struct CharacterColorUsage
{
    // Visible source pixels per Color-1/Color-2 region after head composition.
    std::array<std::array<unsigned, 6>, 2> ready{}, action{};
    [[nodiscard]] bool contains(unsigned bank, unsigned part) const;
};

// Keeps the source color indices: they identify regions, not final RGB colors.
[[nodiscard]] IndexedIcon decode_character_icon(std::span<const std::uint8_t> record);
[[nodiscard]] Image compose_character_icon(const IndexedIcon &head, const IndexedIcon &body,
        const CharacterAppearance &appearance);
[[nodiscard]] std::array<std::uint8_t, 3> character_color(unsigned index);

class CharacterArt
{
  public:
    [[nodiscard]] static CharacterArt load(const std::filesystem::path &directory);
    // The parts are private so each keeps the size the composition relies on:
    // portrait heads 88x40, bodies 88x48, combat icons 24 pixels wide and at
    // most 24 high (Effective C++ Item 22). These add or replace a part after
    // checking it.
    void add_portrait_head(unsigned id, Image image);
    void set_original_head(unsigned id, PortraitPart part);
    void set_original_body(unsigned id, PortraitPart part);
    void set_combat_head(unsigned id, IndexedIcon icon);
    void set_combat_body(unsigned id, IndexedIcon icon);

    [[nodiscard]] const std::map<unsigned, PortraitPart> &heads() const
    {
        return heads_;
    }

    [[nodiscard]] const std::map<unsigned, PortraitPart> &bodies() const
    {
        return bodies_;
    }

    [[nodiscard]] const std::map<unsigned, IndexedIcon> &combat_heads() const
    {
        return combat_heads_;
    }

    [[nodiscard]] const std::map<unsigned, IndexedIcon> &combat_bodies() const
    {
        return combat_bodies_;
    }

    [[nodiscard]] Image portrait(const CharacterAppearance &) const;
    [[nodiscard]] Image icon(const CharacterAppearance &, IconPose pose) const;
    // Stable anatomy from the saved body; only the wielding arms and equipment
    // come from the catalog pose. Original archives are decoded in memory.
    [[nodiscard]] IndexedIcon combat_anatomy(const CharacterAppearance &, IconPose pose) const;
    [[nodiscard]] Image equipped_icon(const CharacterAppearance &, unsigned equipment_body,
                                      IconPose pose) const;
    [[nodiscard]] CharacterColorUsage color_usage(const CharacterAppearance &) const;
    void validate(const CharacterAppearance &) const;

  private:
    std::map<unsigned, PortraitPart> heads_, bodies_;
    std::map<unsigned, IndexedIcon> combat_heads_, combat_bodies_;
};
} // namespace opengold::por
#endif
