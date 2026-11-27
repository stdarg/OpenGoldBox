#include "opengold/message.h"

#include <algorithm>

namespace opengold::rules
{
namespace
{
// The first argument whose {name} starts the template at `at`, or none.
const MessageArgument *placeholder_at(const Message &message, std::size_t at)
{
    if (message.source[at] != '{')
    {
        return nullptr;
    }
    const auto found = std::find_if(message.arguments.begin(), message.arguments.end(),
                                    [&](const MessageArgument &argument)
                                    {
                                        const auto key = "{" + argument.name + "}";
                                        return message.source.compare(at, key.size(), key) == 0;
                                    });
    return found == message.arguments.end() ? nullptr : &*found;
}
} // namespace

std::string english(const Message &message)
{
    // One pass over the template: an inserted value is never searched again,
    // so a character the player named "{target}" keeps that name instead of
    // taking the target's.
    std::string text;
    std::size_t at = 0;
    while (at < message.source.size())
    {
        const auto *argument = placeholder_at(message, at);
        if (argument == nullptr)
        {
            text += message.source[at];
            ++at;
            continue;
        }
        text += argument->value;
        at += argument->name.size() + 2;
    }
    return text;
}
} // namespace opengold::rules
