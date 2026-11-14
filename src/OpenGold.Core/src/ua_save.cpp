// Saving and loading a design's play (docs/games/ua.md, Saving and loading a
// game). The campaign save keeps the party; this is the rest of a game in
// progress, the session's own state, as text the campaign save carries.
#include "opengold/ua_session.h"

#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>

namespace opengold::ua
{
namespace
{

// The first line of a saved state, naming its layout.
constexpr std::string_view state_tag = "UA-STATE 1";

void require(bool ok, const char *message)
{
    if (!ok)
    {
        throw std::runtime_error(message);
    }
}

// The events done, four to a hexadecimal digit, the first in the top bit.
std::string hex_bits(const std::vector<bool> &bits)
{
    constexpr std::string_view digits = "0123456789abcdef";
    std::string text;
    for (std::size_t n = 0; n < bits.size(); n += 4)
    {
        unsigned digit = 0;
        for (std::size_t bit = 0; bit < 4; ++bit)
        {
            digit = digit << 1 | (n + bit < bits.size() && bits[n + bit] ? 1U : 0U);
        }
        text += digits[digit];
    }
    return text;
}

std::vector<bool> bits_from_hex(std::string_view text, std::size_t count)
{
    require(text.size() == (count + 3) / 4, "The saved design's events do not fit");
    std::vector<bool> bits(count);
    for (std::size_t n = 0; n < count; ++n)
    {
        const char c = text[n / 4];
        const auto at = std::string_view("0123456789abcdef").find(c);
        require(at != std::string_view::npos, "The saved design's events are not readable");
        bits[n] = (at >> (3 - n % 4) & 1U) != 0;
    }
    return bits;
}

// Reads one value of the saved state, failing on anything else.
template <class T> T next(std::istream &in)
{
    T value{};
    in >> value;
    require(static_cast<bool>(in), "The saved design's state is cut short");
    return value;
}

por::MapDirection facing_from(unsigned number)
{
    require(number < 4, "The saved design's facing is not a direction");
    return por::MapDirection{number};
}

} // namespace

std::string UaSession::saved_state() const
{
    if (!can_leave() || encounter_ || party_lost_ || running_)
    {
        throw std::logic_error("A design is saved only while nothing waits for the player");
    }
    std::ostringstream out;
    out.imbue(std::locale::classic());
    const auto &pose = snapshot_.pose;
    out << state_tag << '\n'
        << area_.number << ' ' << pose.x << ' ' << pose.y << ' ' << por::index(pose.facing) << '\n';
    const auto back = step_back_pose_.value_or(pose);
    out << back.x << ' ' << back.y << ' ' << por::index(back.facing) << '\n';
    out << started_ << ' ' << snapshot_.searching << ' ' << snapshot_.footsteps << ' '
        << clock_minutes_ << ' ' << dice_ << '\n';
    out << donated_ << ' ' << vault_[0] << ' ' << vault_[1] << ' ' << vault_[2] << ' '
        << tavern_tale_next_ << ' ' << drunkenness_ << '\n';
    for (const auto flag : quest_flags_)
    {
        out << unsigned{flag} << ' ';
    }
    out << '\n' << hex_bits(done_bits_) << '\n';
    out << snapshot_.visited.to_string() << ' ' << snapshot_.seen.to_string() << '\n';
    out << tavern_tales_.size();
    for (const auto &tale : tavern_tales_)
    {
        out << ' ' << std::quoted(tale);
    }
    out << '\n';
    return out.str();
}

UaSession UaSession::resume(DesignFiles files, std::string_view state)
{
    std::istringstream in{std::string(state)};
    in.imbue(std::locale::classic());
    std::string tag;
    std::getline(in, tag);
    require(tag == state_tag, "The saved design's state is not one this build reads");
    auto session = start(std::move(files));
    const auto area = next<unsigned>(in);
    if (area != session.area_.number)
    {
        session.load_area(area);
    }
    const auto &map = session.area_.area;
    const auto square = [&map](unsigned x, unsigned y)
    {
        require(x < map.columns && y < map.rows, "The saved square is outside its area");
    };
    const auto x = next<unsigned>(in), y = next<unsigned>(in);
    square(x, y);
    session.move_party(x, y, facing_from(next<unsigned>(in)));
    por::PartyPose back;
    back.x = next<unsigned>(in);
    back.y = next<unsigned>(in);
    square(back.x, back.y);
    back.facing = facing_from(next<unsigned>(in));
    session.step_back_pose_ = back;
    session.started_ = next<bool>(in);
    session.snapshot_.searching = next<bool>(in);
    session.snapshot_.footsteps = next<decltype(session.snapshot_.footsteps)>(in);
    session.clock_minutes_ = next<std::uint64_t>(in);
    session.dice_ = next<std::uint32_t>(in);
    session.donated_ = next<std::uint32_t>(in);
    for (auto &stored : session.vault_)
    {
        stored = next<std::uint32_t>(in);
    }
    session.tavern_tale_next_ = next<unsigned>(in);
    session.drunkenness_ = next<unsigned>(in);
    for (auto &flag : session.quest_flags_)
    {
        const auto value = next<unsigned>(in);
        require(value <= 0xFF, "A saved quest flag is out of range");
        flag = static_cast<std::uint8_t>(value);
    }
    session.done_bits_ = bits_from_hex(next<std::string>(in), session.done_bits_.size());
    session.snapshot_.visited = decltype(session.snapshot_.visited)(next<std::string>(in));
    session.snapshot_.seen = decltype(session.snapshot_.seen)(next<std::string>(in));
    const auto tales = next<std::size_t>(in);
    require(tales <= 100, "The saved tavern tales are too many");
    for (std::size_t n = 0; n < tales; ++n)
    {
        std::string tale;
        in >> std::quoted(tale);
        require(static_cast<bool>(in), "The saved design's state is cut short");
        session.tavern_tales_.push_back(std::move(tale));
    }
    in >> std::ws;
    require(in.eof(), "The saved design's state has more than it should");
    session.refresh_snapshot();
    return session;
}

} // namespace opengold::ua
