#ifndef OPENGOLD_MESSAGE_H
#define OPENGOLD_MESSAGE_H
#include <string>
#include <vector>

namespace opengold::rules
{
// Presentation data only. Rules emit source templates and typed arguments; the
// host chooses a language. A literal argument (names, numbers, IDs) is never
// looked up in a translation catalog.
struct MessageArgument
{
    std::string name, value;
    bool translate{};
};
struct Message
{
    std::string source;
    std::vector<MessageArgument> arguments;
};
} // namespace opengold::rules
#endif
