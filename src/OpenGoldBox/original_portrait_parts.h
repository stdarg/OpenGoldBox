#ifndef OPENGOLDBOX_ORIGINAL_PORTRAIT_PARTS_H
#define OPENGOLDBOX_ORIGINAL_PORTRAIT_PARTS_H

#include "opengold/character_art.h"
#include "opengold/character_rules.h"
#include <godot_cpp/classes/node.hpp>
#include <cstdint>

// The head and body buttons that build an original portrait
// (scenes/original_portrait_parts.tscn), shared by character creation and the
// sheet's portrait dialog so both step the same parts.
namespace presentation
{
enum class OriginalPart
{
    head,
    body
};

// The appearance with its head (among those the draft's race and gender may
// wear) or body stepped `direction` places, wrapping around.
[[nodiscard]] opengold::por::CharacterAppearance stepped_original_part(
    opengold::por::CharacterAppearance appearance, const opengold::por::CharacterArt &art,
    const opengold::rules::CharacterDraft &draft, OriginalPart part, std::int64_t direction);

// Writes which head and body the appearance wears into the parts' labels.
void show_original_parts(godot::Node &parts, const opengold::por::CharacterArt &art,
                         const opengold::rules::CharacterDraft &draft,
                         const opengold::por::CharacterAppearance &appearance);
} // namespace presentation

#endif
