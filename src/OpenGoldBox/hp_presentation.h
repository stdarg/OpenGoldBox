#ifndef OPENGOLD_HP_PRESENTATION_H
#define OPENGOLD_HP_PRESENTATION_H
#include "localization.h"
#include "opengold/rules.h"
namespace presentation
{
inline godot::String hp_color(int hp, int maximum)
{
    return hp <= 0 || std::int64_t(hp) * 5 <= maximum ? "#f08080"
           : hp < maximum                             ? "#f3d55b"
                                                      : "#80d99a";
}
inline godot::String bbcode_literal(const godot::String &value)
{
    return value.replace("[", "[lb]");
}
inline godot::String temporary_hp_source(const opengold::rules::TemporaryHitPoints &pool)
{
    return pool.source_id == "species:orc/trait:adrenaline_rush"
               ? i18n::text("Orc / Adrenaline Rush")
               : godot::String::utf8(pool.source_id.c_str());
}
inline godot::String hp_hint(int hp, int maximum, bool dead,
                             const std::vector<opengold::rules::Message> &sources)
{
    auto hint = i18n::render(sources);
    if (hp < maximum)
    {
        if (!hint.is_empty())
            hint += "\n";
        hint += i18n::format(
            "Missing HP: {amount}. Damage and other HP losses are not individually recorded.",
            {{"amount", maximum - hp}});
    }
    if (dead || hp == 0)
    {
        if (!hint.is_empty())
            hint += "\n";
        hint += i18n::text(dead ? "Dead" : "Unconscious");
    }
    if (hint.is_empty())
        hint = i18n::text("Full hit points.");
    return hint;
}
inline godot::String hp_hint_tag(godot::String hint, const godot::String &body)
{
    // Hint attributes have no quote escape in Godot. Use typographic quotation
    // and parentheses so names/source strings cannot inject BBCode tags.
    hint = hint.replace("\"", "’").replace("[", "(").replace("]", ")");
    return "[hint=\"" + hint + "\"]" + body + "[/hint]";
}
inline godot::String hp_text(int hp, int maximum, bool dead,
                             const opengold::rules::TemporaryHitPoints &pool,
                             const std::vector<opengold::rules::Message> &sources)
{
    auto text = hp_hint_tag(
        hp_hint(hp, maximum, dead, sources),
        "[color=" + hp_color(hp, maximum) + "]" +
            i18n::format("HP {current} / {maximum}", {{"current", hp}, {"maximum", maximum}}) +
            "[/color]");
    if (pool.amount)
    {
        const auto hint = i18n::format(
            "Temporary HP from {source}. Absorbs damage before ordinary HP; does not heal or stack.",
            {{"source", temporary_hp_source(pool)}});
        text += "   " +
                hp_hint_tag(hint, "[color=#80d99a]" +
                                      i18n::format("Temp HP {amount}", {{"amount", pool.amount}}) +
                                      "[/color]");
    }
    return text;
}
} // namespace presentation
#endif
