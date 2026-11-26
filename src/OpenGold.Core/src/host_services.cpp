#include "host_services.h"

namespace opengold::por
{
namespace host_services
{
std::string english(const Message &message)
{
    auto text = message.source;
    for (const auto &argument : message.arguments)
    {
        for (std::size_t at = text.find("{" + argument.name + "}"); at != std::string::npos;
             at = text.find("{" + argument.name + "}"))
        {
            text.replace(at, argument.name.size() + 2, argument.value);
        }
    }
    return text;
}

std::string english(const std::vector<Message> &messages)
{
    std::string text;
    for (const auto &message : messages)
    {
        text += (text.empty() ? "" : "\n") + english(message);
    }
    return text;
}

std::string amount(std::uint32_t value)
{
    auto text = std::to_string(value);
    for (auto at = text.size(); at > 3; at -= 3)
    {
        text.insert(at - 3, ",");
    }
    return text;
}
} // namespace host_services

} // namespace opengold::por
