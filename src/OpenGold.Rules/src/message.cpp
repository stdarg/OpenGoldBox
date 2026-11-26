#include "opengold/message.h"

namespace opengold::rules
{
std::string english(const Message &message)
{
    auto text = message.source;
    for (const auto &argument : message.arguments)
    {
        const auto key = "{" + argument.name + "}";
        // Resuming after the inserted value keeps a value that holds its own
        // {name}, such as a character the player named so, from being found
        // again and replaced forever.
        for (auto at = text.find(key); at != std::string::npos; at = text.find(key, at))
        {
            text.replace(at, key.size(), argument.value);
            at += argument.value.size();
        }
    }
    return text;
}
} // namespace opengold::rules
