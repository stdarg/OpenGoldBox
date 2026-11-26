#ifndef OPENGOLD_HOST_SERVICES_H
#define OPENGOLD_HOST_SERVICES_H

// The text helpers RolfTourSession's town services share: phlan_session.cpp
// and the host_*.cpp files that hold its halls, temples, shops, treasure and
// camp.

#include "opengold/message.h"

#include <cstdint>
#include <string>
#include <vector>

namespace opengold::por::host_services
{
using opengold::rules::Message;

// The English text of host messages, one per line, for logs and tests.
[[nodiscard]] std::string english(const std::vector<Message> &messages);
// Thousands separated as the halls' fees are written ("1,000").
[[nodiscard]] std::string amount(std::uint32_t value);
} // namespace opengold::por::host_services
#endif
