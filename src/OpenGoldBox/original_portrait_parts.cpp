#include "original_portrait_parts.h"
#include "godot_nodes.h"
#include "localization.h"
#include <godot_cpp/classes/label.hpp>
#include <algorithm>
#include <vector>

namespace presentation
{
namespace
{
std::vector<unsigned> part_ids(const opengold::por::CharacterArt &art,
                               const opengold::rules::CharacterDraft &draft, OriginalPart part)
{
    if (part == OriginalPart::head)
    {
        return opengold::por::portrait_heads_for(art, draft.race, draft.gender);
    }
    std::vector<unsigned> ids;
    for (const auto &[id, body] : art.bodies())
    {
        ids.push_back(id);
    }
    return ids;
}

// Where `id` stands among `ids`, from 0; a part they lack stands first.
std::int64_t place(const std::vector<unsigned> &ids, unsigned id)
{
    const auto found = std::ranges::find(ids, id);
    return found == ids.end() ? 0 : found - ids.begin();
}
} // namespace

opengold::por::CharacterAppearance stepped_original_part(
    opengold::por::CharacterAppearance appearance, const opengold::por::CharacterArt &art,
    const opengold::rules::CharacterDraft &draft, OriginalPart part, std::int64_t direction)
{
    const auto ids = part_ids(art, draft, part);
    if (ids.empty())
    {
        return appearance;
    }
    auto &id = part == OriginalPart::head ? appearance.portrait_head : appearance.portrait_body;
    const auto count = static_cast<std::int64_t>(ids.size());
    id = ids[static_cast<std::size_t>((place(ids, id) + direction % count + count) % count)];
    return appearance;
}

void show_original_parts(godot::Node &parts, const opengold::por::CharacterArt &art,
                         const opengold::rules::CharacterDraft &draft,
                         const opengold::por::CharacterAppearance &appearance)
{
    const auto heads = part_ids(art, draft, OriginalPart::head);
    const auto bodies = part_ids(art, draft, OriginalPart::body);
    required_node<godot::Label>(parts, "HeadNumber")
        .set_text(i18n::format(N_("Head {number} of {count}"),
                               {{"number", place(heads, appearance.portrait_head) + 1},
                                {"count", static_cast<std::int64_t>(heads.size())}}));
    required_node<godot::Label>(parts, "BodyNumber")
        .set_text(i18n::format(N_("Body {number} of {count}"),
                               {{"number", place(bodies, appearance.portrait_body) + 1},
                                {"count", static_cast<std::int64_t>(bodies.size())}}));
}
} // namespace presentation
