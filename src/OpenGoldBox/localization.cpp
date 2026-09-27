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
String gs(std::string_view value)
{
    return String::utf8(value.data(), value.size());
}

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
        values[name] = value;
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
        label->set_text(translate_string(label->get_text()));
    else if (auto *rich = Object::cast_to<RichTextLabel>(&root))
        rich->set_text(translate_string(rich->get_text()));
    else if (auto *button = Object::cast_to<Button>(&root))
        button->set_text(translate_string(button->get_text()));
    else if (auto *edit = Object::cast_to<LineEdit>(&root))
        edit->set_placeholder(translate_string(edit->get_placeholder()));
    if (auto *window = Object::cast_to<Window>(&root))
        window->set_title(translate_string(window->get_title()));
    if (auto *control = Object::cast_to<Control>(&root))
        control->set_tooltip_text(translate_string(control->get_tooltip_text()));
    for (int i = 0; i < root.get_child_count(); ++i)
        prepare_ui(*root.get_child(i));
}

String text(std::string_view source)
{
    if (source.empty())
        return {};
    return TranslationServer::get_singleton()->translate(gs(source));
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
        values[name] = value;
    values["count"] = count;
    return interpolate(translated, values);
}

String campaign(std::string_view resource, std::string_view original)
{
    const String translated =
        TranslationServer::get_singleton()->translate(gs(original), gs(resource));
    return translated == gs(original) ? text(original) : translated;
}

String render(const opengold::rules::Message &message)
{
    Dictionary values;
    for (const auto &argument : message.arguments)
        values[gs(argument.name)] = argument.translate ? text(argument.value) : gs(argument.value);
    return interpolate(text(message.source), values);
}

String render(const std::vector<opengold::rules::Message> &messages)
{
    String result;
    for (const auto &message : messages)
    {
        if (!result.is_empty())
            result += "\n";
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
        return false;
    initialize();
    return true;
}
} // namespace i18n
