#include "host_services.h"
#include "opengold/phlan_resources.h"

namespace opengold::por
{
namespace host_services
{
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

std::optional<EclConditions> vga_combat_conditions(GoldBoxGame game, bool party_stood)
{
    if (!vga_engine(game))
    {
        return std::nullopt;
    }
    return EclConditions{party_stood, !party_stood, false, false, false, false};
}

Image close_up_portrait(const PhlanResources &resources, unsigned head_id, unsigned body_id)
{
    const auto head = resources.heads.find(head_id), body = resources.bodies.find(body_id);
    if (head == resources.heads.end() || body == resources.bodies.end() ||
        head->second.width != 88 || body->second.width != 88 || head->second.height != 40 ||
        body->second.height != 48)
    {
        throw EclError("Unsupported town portrait composition");
    }
    Image portrait;
    portrait.width = portrait.height = 88;
    portrait.rgba = head->second.rgba;
    portrait.rgba.insert(portrait.rgba.end(), body->second.rgba.begin(), body->second.rgba.end());
    return portrait;
}

std::uint64_t clock_minute_of_day(std::uint64_t minutes)
{
    return (minutes % 1440 + 720) % 1440;
}
} // namespace host_services

} // namespace opengold::por
