#include "opengold/character_pool.h"
#include <algorithm>
#include <numeric>
#include <stdexcept>

namespace opengold
{
namespace
{
// Match dominant non-background portrait colors to the original sprite palette.
unsigned dominant(const Image &image, unsigned first_row, unsigned last_row, unsigned alternate = 0)
{
    std::array<unsigned, 16> counts{};
    for (unsigned y = first_row; y < std::min<unsigned>(last_row, image.height); ++y)
        for (unsigned x = 8; x + 8 < image.width; ++x)
        {
            const auto p = (y * image.width + x) * 4;
            if (image.rgba[p + 3] < 128 ||
                    std::max({image.rgba[p], image.rgba[p + 1], image.rgba[p + 2]}) < 40)
                continue;
            unsigned nearest = 0;
            int distance = 200000;
            for (unsigned c = 1; c < 16; ++c)
            {
                const auto rgb = por::character_color(c);
                int error = 0;
                for (unsigned k = 0; k < 3; ++k)
                {
                    const int delta = int(rgb[k]) - image.rgba[p + k];
                    error += delta * delta;
                }
                if (error < distance)
                {
                    distance = error;
                    nearest = c;
                }
            }
            if (nearest)
                ++counts[nearest];
        }
    std::array<unsigned, 16> order{};
    std::iota(order.begin(), order.end(), 0);
    std::stable_sort(order.begin(), order.end(),
                     [&](auto a, auto b)
    {
        return counts[a] > counts[b];
    });
    return counts[order[alternate]] ? order[alternate] : order[0];
}
} // namespace

std::vector<Character> character_pool(const rules::CharacterRules &rules,
                                      const por::CharacterArt &art)
{
    using namespace rules;
    if (art.bodies.empty() || art.heads.empty())
        throw std::runtime_error("Load character art before opening the pool");
    const auto classes = rules.choices(CreationField::character_class);
    const auto alignments = rules.choices(CreationField::alignment);
    // Every pool member has a distinct given name and surname.
    const std::array<const char *, 48> first
    {
        "Arlen",  "Mira",  "Toren",  "Selene", "Borin",   "Lyra",   "Cedric",  "Nessa",
        "Dorian", "Elara", "Finn",   "Mara",   "Garrick", "Isolde", "Hadrian", "Jessa",
        "Ivor",   "Kaela", "Joren",  "Liora",  "Kael",    "Maeve",  "Lucan",   "Nadia",
        "Merric", "Orla",  "Nolan",  "Petra",  "Oren",    "Quinn",  "Perrin",  "Rhea",
        "Rolan",  "Sable", "Silas",  "Talia",  "Theron",  "Una",    "Ulric",   "Vera",
        "Valen",  "Wren",  "Willem", "Xara",   "Yoren",   "Ysolde", "Zev",     "Zinnia"};
    const std::array<const char *, 48> surnames
    {
        "Ashfall",      "Songbrook",   "Dawnward",   "Greenbough",  "Ironvale",    "Stillwater",
        "Brightshield", "Thornpath",   "Nightwind",  "Emberheart",  "Duskwatch",   "Starweave",
        "Amberforge",   "Bramblewood", "Cinderhill", "Deepwell",    "Eaglecrest",  "Frostmere",
        "Goldbranch",   "Hallowgrove", "Ivorythorn", "Juniperfell", "Kestrelwing", "Lightfoot",
        "Mistvale",     "Northwick",   "Oakenshade", "Pinehollow",  "Quickwater",  "Ravencrest",
        "Silverleaf",   "Thistledown", "Umberstone", "Valeguard",   "Westbrook",   "Yarrowfield",
        "Zephyrwind",   "Coppervein",  "Driftwood",  "Evenvale",    "Foxglove",    "Grayhaven",
        "Hazelridge",   "Kingswell",   "Larkspur",   "Moonwhisper", "Reedwalker",  "Stormglen"};
    const std::array<const char *, 5> races{"gnome", "orc", "goliath", "tiefling", "dragonborn"};
    std::vector<unsigned> bodies;
    for (const auto &[id, body] : art.bodies)
        bodies.push_back(id);
    std::vector<Character> result;
    for (unsigned c = 0; c < classes.size(); ++c)
        for (unsigned variant = 0; variant < 4; ++variant)
        {
            CharacterDraft d;
            d.character_class = classes[c].id;
            d.race = races[(c + variant) % races.size()];
            d.gender = variant % 2 ? "female" : "male";
            d.alignment = alignments[(c + variant * 2) % alignments.size()].id;
            const auto name_index = c * 4 + variant;
            d.name = std::string(first.at(name_index)) + " " + surnames.at(name_index);
            d.rolled = true;
            const auto priority = rules.preset_ability_priority(d.character_class, variant);
            const auto primary = priority[0], secondary = priority[1];
            for (unsigned rank = 0; rank < 6; ++rank)
            {
                const int score = 18 - int(rank) - (variant == 3 && rank == 0 ? 1 : 0);
                // Authored, valid 4d6-drop-lowest provenance for each strong score.
                d.rolls[priority[rank]] =
                {
                    {6, score >= 17 ? 6 : 5, score - 6 - (score >= 17 ? 6 : 5), 1}, 3
                };
            }
            d.background = primary == 0   ? "soldier"
                           : primary == 1 ? "criminal"
                           : primary == 3 ? "sage"
                           : "acolyte";
            const auto bonuses = rules.adjustments(d.background);
            for (unsigned a = 0; a < bonuses.size(); ++a)
                if (bonuses[a].bonuses[primary] == 2)
                {
                    d.adjustment = a;
                    if (bonuses[a].bonuses[secondary])
                        break;
                }
            // Presets include deterministic authored choices. Refresh each group's
            // options after its prerequisites, including skills before Expertise.
            const auto groups = rules.training_options(d);
            for (const auto &requested : groups)
            {
                const auto current = rules.training_options(d);
                const auto group = std::find_if(current.begin(), current.end(),
                                                [&](const auto & g)
                {
                    return g.id == requested.id;
                });
                if (group == current.end() || group->options.size() < group->count)
                    throw std::runtime_error("Incomplete preset training options");
                for (unsigned n = 0; n < group->count; ++n)
                    d.training[group->id].push_back(
                        group->options[(name_index + n) % group->options.size()].id);
            }
            const auto cantrips = rules.cantrip_options(d);
            if (cantrips.count)
            {
                d.cantrips.emplace();
                for (const auto &option : cantrips.options)
                {
                    if (d.cantrips->size() == cantrips.count)
                        break;
                    d.cantrips->push_back(option.id);
                }
            }
            const auto spell_options = rules.spell_choice_options(d);
            if (spell_options.may_prepare)
            {
                d.spells = SpellChoices{};
                for (const auto &group : spell_options.learning)
                    for (const auto &option : group.options)
                    {
                        auto &values = d.spells->learning[group.id];
                        if (values.size() == group.count)
                            break;
                        values.push_back(option.id);
                    }
                d.spells->prepared.emplace();
                for (const auto &option : rules.spell_choice_options(d).preparation)
                {
                    if (d.spells->prepared->size() == spell_options.prepared_count)
                        break;
                    d.spells->prepared->push_back(option.id);
                }
            }
            por::CharacterAppearance appearance;
            appearance.portrait_head =
                por::matching_portrait_head(d.race, d.gender).value_or(art.heads.begin()->first);
            if (!art.heads.contains(appearance.portrait_head))
                appearance.portrait_head = art.heads.begin()->first;
            appearance.portrait_body = bodies[(c * 3 + variant) % bodies.size()];
            appearance.combat_head = (c + variant * 3) % 14;
            appearance.combat_body = (c * 2 + variant * 5) % 32;
            appearance.tall = d.race != "gnome";
            const auto portrait = art.portrait(appearance);
            const auto cloth = dominant(portrait, 40, portrait.height),
                       trim = dominant(portrait, 40, portrait.height, 1),
                       skin = dominant(portrait, 14, 38);
            appearance.colors =
            {
                {{7, cloth, skin, trim, cloth, trim}, {15, trim, skin, cloth, trim, cloth}}
            };
            art.validate(appearance);
            result.emplace_back(rules, std::move(d), appearance);
        }
    return result;
}
} // namespace opengold
