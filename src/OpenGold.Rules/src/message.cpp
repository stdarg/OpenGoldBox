#include "opengold/message.h"

namespace opengold::rules
{
std::string english(const Message &message)
{
    auto text = message.source;
    for (const auto &argument : message.arguments)
    {
        const auto key = "{" + argument.name + "}";
        for (auto at = text.find(key); at != std::string::npos; at = text.find(key))
        {
            text.replace(at, key.size(), argument.value);
        }
    }
    return text;
}
} // namespace opengold::rules
