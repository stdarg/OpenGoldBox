#include "godot_text.h"
#include "localization.h"
#include "application_settings.h"
#include <godot_cpp/classes/config_file.hpp>
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/line_edit.hpp>
#include <godot_cpp/classes/rich_text_label.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/translation_server.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

namespace
{
using presentation::gs;

// Substitute once over the template. Braces inside a player-entered name are
// literal data and must never be interpreted as another format expression.
String interpolate(const String &message, const Dictionary &values)
{
    String result;
    int64_t offset = 0;
    while (offset < message.length())
    {
        const auto begin = message.find("{", offset);
        if (begin < 0)
        {
            result += message.substr(offset);
            break;
        }
        result += message.substr(offset, begin - offset);
        const auto end = message.find("}", begin + 1);
        if (end < 0)
        {
            result += message.substr(begin);
            break;
        }
        const auto name = message.substr(begin + 1, end - begin - 1);
        result += values.has(name) ? String(values[name]) : message.substr(begin, end - begin + 1);
        offset = end + 1;
    }
    return result;
}

String supported(const String &locale)
{
    return locale.replace("-", "_").get_slice("_", 0).to_lower() == "es" ? "es" : "en";
}

String substitute(const String &message, std::initializer_list<i18n::Argument> arguments)
{
    Dictionary values;
    for (const auto &[name, value] : arguments)
    {
        values[name] = value;
    }
    return interpolate(message, values);
}
} // namespace

namespace i18n
{
void prepare_ui(Node &root)
{
    root.set_auto_translate_mode(Node::AUTO_TRANSLATE_MODE_DISABLED);
    const auto translate_string = [](const String &value)
    {
        return text(value.utf8().get_data());
    };
    if (auto *label = Object::cast_to<Label>(&root))
    {
        label->set_text(translate_string(label->get_text()));
    }
    else if (auto *rich = Object::cast_to<RichTextLabel>(&root))
    {
        rich->set_text(translate_string(rich->get_text()));
    }
    else if (auto *button = Object::cast_to<Button>(&root))
    {
        button->set_text(translate_string(button->get_text()));
    }
    else if (auto *edit = Object::cast_to<LineEdit>(&root))
    {
        edit->set_placeholder(translate_string(edit->get_placeholder()));
    }
    if (auto *window = Object::cast_to<Window>(&root))
    {
        window->set_title(translate_string(window->get_title()));
    }
    if (auto *control = Object::cast_to<Control>(&root))
    {
        control->set_tooltip_text(translate_string(control->get_tooltip_text()));
    }
    for (int i = 0; i < root.get_child_count(); ++i)
    {
        prepare_ui(*root.get_child(i));
    }
}

String text(std::string_view source)
{
    if (source.empty())
    {
        return {};
    }
    const String translated = TranslationServer::get_singleton()->translate(gs(source));
    // "Longsword +2", an enchanted item, is its translated label and its bonus.
    const auto size = source.size();
    if (translated == gs(source) && size > 3 && source[size - 3] == ' ' &&
        (source[size - 2] == '+' || source[size - 2] == '-') && source[size - 1] >= '1' &&
        source[size - 1] <= '9')
    {
        return text(source.substr(0, size - 3)) + gs(source.substr(size - 3));
    }
    // "Flame Strike (Divine Intervention)", a spell cast through a Cleric's
    // Divine Intervention, is translated in two parts.
    constexpr std::string_view intervention = " (Divine Intervention)";
    if (translated == gs(source) && source.size() > intervention.size() &&
        source.ends_with(intervention))
    {
        return format(N_("{spell} (Divine Intervention)"),
                      {{"spell", text(source.substr(0, source.size() - intervention.size()))}});
    }
    // "Eyebite (Mystic Arcanum)", a Warlock's once-a-day spell, is
    // translated in two parts too.
    constexpr std::string_view arcanum = " (Mystic Arcanum)";
    if (translated == gs(source) && source.size() > arcanum.size() && source.ends_with(arcanum))
    {
        return format(N_("{spell} (Mystic Arcanum)"),
                      {{"spell", text(source.substr(0, source.size() - arcanum.size()))}});
    }
    // "Fireball (Natural Recovery)", a Druid's free Circle Spell, likewise.
    constexpr std::string_view natural = " (Natural Recovery)";
    if (translated == gs(source) && source.size() > natural.size() && source.ends_with(natural))
    {
        return format(N_("{spell} (Natural Recovery)"),
                      {{"spell", text(source.substr(0, source.size() - natural.size()))}});
    }
    // "Dimension Door with Ilsa", a spell that takes a companion, is
    // translated in two parts: the companion is a name.
    constexpr std::string_view companion = "Dimension Door with ";
    if (translated == gs(source) && source.starts_with(companion) &&
        source.size() > companion.size() && !source.ends_with(" slot)"))
    {
        return format(N_("{spell} with {name}"), {{"spell", text("Dimension Door")},
                                                  {"name", gs(source.substr(companion.size()))}});
    }
    // "Cure Wounds (level 3 slot)", a spell cast from a higher slot, is
    // translated in two parts when the whole has no entry of its own.
    constexpr std::string_view slot_prefix = " (level ", slot_suffix = " slot)";
    const auto at = source.rfind(slot_prefix);
    if (translated != gs(source) || at == std::string_view::npos ||
        !source.ends_with(slot_suffix) ||
        source.size() != at + slot_prefix.size() + 1 + slot_suffix.size())
    {
        return translated;
    }
    const char level = source[at + slot_prefix.size()];
    if (level < '2' || level > '9')
    {
        return translated;
    }
    return format(N_("{spell} (level {level} slot)"),
                  {{"spell", text(source.substr(0, at))}, {"level", String::chr(level)}});
}

std::string utf8(std::string_view source)
{
    return text(source).utf8().get_data();
}

String format(std::string_view source, std::initializer_list<Argument> arguments)
{
    return substitute(text(source), arguments);
}

std::string formatted(std::string_view source, std::initializer_list<Argument> arguments)
{
    return format(source, arguments).utf8().get_data();
}

String plural(std::string_view singular, std::string_view multiple, int count,
              std::initializer_list<Argument> arguments)
{
    const auto translated =
        TranslationServer::get_singleton()->translate_plural(gs(singular), gs(multiple), count);
    Dictionary values;
    for (const auto &[name, value] : arguments)
    {
        values[name] = value;
    }
    values["count"] = count;
    return interpolate(translated, values);
}

String campaign(std::string_view resource, std::string_view original)
{
    const String translated =
        TranslationServer::get_singleton()->translate(gs(original), gs(resource));
    return translated == gs(original) ? text(original) : translated;
}

namespace
{
// A number the rules wrote with English thousands commas ("1,000"), regrouped
// with the language's separator ("1.000" in Spanish).
String grouped_number(const std::string &value)
{
    const bool grouped =
        value.size() > 4 && value.size() % 4 != 0 &&
        std::all_of(value.begin(), value.end(),
                    [&](char c)
                    {
                        return std::isdigit(static_cast<unsigned char>(c)) || c == ',';
                    }) &&
        [&]
    {
        for (std::size_t at = value.size() - 4;; at -= 4)
        {
            if (value[at] != ',')
            {
                return false;
            }
            if (at < 4)
            {
                return value.find(',') == at;
            }
        }
    }();
    // The language's thousands separator, from how it writes a grouped number.
    const auto separator = text(N_("{high},{low}")).replace("{high}", "").replace("{low}", "");
    return grouped ? gs(value).replace(",", separator) : gs(value);
}
} // namespace

String render(const opengold::rules::Message &message)
{
    Dictionary values;
    for (const auto &argument : message.arguments)
    {
        values[gs(argument.name)] =
            argument.translate ? text(argument.value) : grouped_number(argument.value);
    }
    return interpolate(text(message.source), values);
}

String render(const std::vector<opengold::rules::Message> &messages)
{
    String result;
    for (const auto &message : messages)
    {
        if (!result.is_empty())
        {
            result += "\n";
        }
        result += render(message);
    }
    return result;
}

String language()
{
    return supported(TranslationServer::get_singleton()->get_locale());
}

void initialize()
{
    TranslationServer::get_singleton()->set_locale(settings::language());
}

bool select_language(const String &locale)
{
    if (!settings::save_language(locale))
    {
        return false;
    }
    initialize();
    return true;
}
} // namespace i18n
