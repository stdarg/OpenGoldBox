#ifndef OPENGOLDBOX_LOCALIZATION_H
#define OPENGOLDBOX_LOCALIZATION_H

#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include "opengold/message.h"
namespace godot
{
class Node;
}

// Marks static source messages for the catalog extractor without translating IDs.
#define N_(message) message

namespace i18n
{
using Argument = std::pair<const char *, godot::Variant>;
godot::String text(std::string_view source);
std::string utf8(std::string_view source);
godot::String format(std::string_view source, std::initializer_list<Argument> arguments);
std::string formatted(std::string_view source, std::initializer_list<Argument> arguments);
godot::String plural(std::string_view singular, std::string_view plural, int count,
                     std::initializer_list<Argument> arguments = {});
// Campaign resources use an explicit stable context; untranslated resources retain
// the original, locally decoded text. Never translate resource IDs or saved names.
godot::String campaign(std::string_view resource, std::string_view original);
godot::String render(const opengold::rules::Message &message);
godot::String render(const std::vector<opengold::rules::Message> &messages);
godot::String language();
void initialize();
bool select_language(const godot::String &locale);
// Translate scene-authored labels once, then let native renderers own text.
// Prevents a second translation of formatted messages and player-entered names.
void prepare_ui(godot::Node &root);
} // namespace i18n
#endif
