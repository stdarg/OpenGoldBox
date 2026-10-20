#ifndef OPENGOLD_STATUS_EFFECT_TEXT_H
#define OPENGOLD_STATUS_EFFECT_TEXT_H
#include "godot_text.h"
#include "localization.h"
#include "opengold/rules.h"

// A status effect as the character sheet and the combat panel show it.
namespace presentation
{
// How long the effect has left: rounds within a minute, then minutes, hours
// and days, each rounded up so an effect never reads as already over.
inline godot::String effect_time_left(const opengold::rules::StatusEffectView &effect)
{
    using opengold::rules::EffectEnd;
    if (effect.end == EffectEnd::long_rest)
    {
        return i18n::text("until a Long Rest");
    }
    if (effect.end == EffectEnd::removed)
    {
        return i18n::text("until magic undoes it");
    }
    if (effect.end == EffectEnd::untimed)
    {
        return {};
    }
    const auto left = effect.remaining.count();
    const auto rounded_up = [left](std::int64_t unit)
    {
        return static_cast<int>((left + unit - 1) / unit);
    };
    if (left <= 60'000)
    {
        return i18n::plural("{count} round left", "{count} rounds left", rounded_up(6'000));
    }
    if (left <= 3'600'000)
    {
        return i18n::plural("{count} minute left", "{count} minutes left", rounded_up(60'000));
    }
    if (left <= 172'800'000)
    {
        return i18n::plural("{count} hour left", "{count} hours left", rounded_up(3'600'000));
    }
    return i18n::plural("{count} day left", "{count} days left", rounded_up(86'400'000));
}

// "Aid (from Ilsa) — 8 hours left", and what it does on the next line when
// `describe` is set.
inline godot::String status_effect_text(const opengold::rules::StatusEffectView &effect,
                                        bool describe)
{
    auto text = i18n::render(effect.name);
    if (!effect.source.empty())
    {
        text = i18n::format("{effect} (from {source})",
                            {{"effect", text}, {"source", gs(effect.source)}});
    }
    if (const auto time = effect_time_left(effect); !time.is_empty())
    {
        text = i18n::format("{effect} — {time}", {{"effect", text}, {"time", time}});
    }
    if (describe)
    {
        text += "\n" + i18n::render(effect.description);
    }
    return text;
}

// The effects one after another, each on its own line.
inline godot::String
status_effect_lines(const std::vector<opengold::rules::StatusEffectView> &effects, bool describe)
{
    godot::String text;
    for (const auto &effect : effects)
    {
        if (!text.is_empty())
        {
            text += "\n";
        }
        text += status_effect_text(effect, describe);
    }
    return text;
}
} // namespace presentation
#endif
