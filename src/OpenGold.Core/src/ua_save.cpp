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

// An item left in the vault, its record's numbers in a line.
void write_item(std::ostream &out, const por::ItemRecord &item)
{
    out << unsigned{item.type};
    for (const auto word : item.name_components)
    {
        out << ' ' << unsigned{word};
    }
    out << ' ' << item.weight << ' ' << item.value << ' ' << int{item.magic_bonus} << ' '
        << int{item.save_bonus} << ' ' << unsigned{item.hidden_name_words} << ' '
        << unsigned{item.cursed_raw} << ' ' << unsigned{item.stack_size};
    for (const auto code : item.effect_codes)
    {
        out << ' ' << unsigned{code};
    }
    out << '\n';
}

template <class T> T next_in(std::istream &in, int lowest, int highest)
{
    const auto value = next<int>(in);
    require(value >= lowest && value <= highest, "A saved vault item is out of range");
    return static_cast<T>(value);
}

por::ItemRecord read_item(std::istream &in)
{
    por::ItemRecord item;
    item.type = next_in<std::uint8_t>(in, 0, 0xFF);
    for (auto &word : item.name_components)
    {
        word = next_in<std::uint8_t>(in, 0, 0xFF);
    }
    item.weight = next_in<std::uint16_t>(in, 0, 0xFFFF);
    item.value = next_in<std::uint16_t>(in, 0, 0xFFFF);
    item.magic_bonus = next_in<std::int8_t>(in, -128, 127);
    item.save_bonus = next_in<std::int8_t>(in, -128, 127);
    item.hidden_name_words = next_in<std::uint8_t>(in, 0, 0xFF);
    item.cursed_raw = next_in<std::uint8_t>(in, 0, 0xFF);
    item.stack_size = next_in<std::uint8_t>(in, 0, 0xFF);
    for (auto &code : item.effect_codes)
    {
        code = next_in<std::uint8_t>(in, 0, 0xFF);
    }
    return item;
}

por::MapDirection facing_from(unsigned number)
{
    require(number < 4, "The saved design's facing is not a direction");
    return por::MapDirection{number};
}

} // namespace

std::string UaSession::saved_state() const
{
    if (!can_leave() || encounter_ || (party_lost_ && !adventure_won_) || running_)
    {
        throw std::logic_error("A design is saved only while nothing waits for the player");
    }
    std::ostringstream out;
    out.imbue(std::locale::classic());
    const auto &pose = snapshot_.pose;
    out << saved_state_format << '\n'
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
    out << hex_bits(stood_on_) << '\n';
    for (const auto steps : step_counts_)
    {
        out << steps << ' ';
    }
    out << '\n';
    out << tavern_tales_.size();
    for (const auto &tale : tavern_tales_)
    {
        out << ' ' << std::quoted(tale);
    }
    out << '\n' << vault_items_.size() << '\n';
    for (const auto &item : vault_items_)
    {
        write_item(out, item);
    }
    out << adventure_won_ << '\n';
    return out.str();
}

UaSession UaSession::resume(DesignFiles files, std::string_view state)
{
    std::istringstream in{std::string(state)};
    in.imbue(std::locale::classic());
    std::string tag;
    std::getline(in, tag);
    require(tag == saved_state_format, "The saved design's state is not one this build reads");
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
    const auto saved_x = next<unsigned>(in), saved_y = next<unsigned>(in);
    square(saved_x, saved_y);
    session.move_party(saved_x, saved_y, facing_from(next<unsigned>(in)));
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
    session.stood_on_ = bits_from_hex(next<std::string>(in), session.stood_on_.size());
    session.refresh_map_window();
    for (auto &steps : session.step_counts_)
    {
        steps = next<unsigned>(in);
    }
    const auto tales = next<std::size_t>(in);
    require(tales <= 100, "The saved tavern tales are too many");
    for (std::size_t n = 0; n < tales; ++n)
    {
        std::string tale;
        in >> std::quoted(tale);
        require(static_cast<bool>(in), "The saved design's state is cut short");
        session.tavern_tales_.push_back(std::move(tale));
    }
    const auto kept = next<std::size_t>(in);
    require(kept <= 50, "The saved vault holds too many items");
    for (std::size_t n = 0; n < kept; ++n)
    {
        const auto item = read_item(in);
        require(item.type < session.items_.types.size(), "A saved vault item has no type");
        session.vault_items_.push_back(item);
    }
    if (next<bool>(in))
    {
        session.adventure_won_ = session.party_lost_ = true;
        session.end_events();
    }
    in >> std::ws;
    require(in.eof(), "The saved design's state has more than it should");
    session.refresh_snapshot();
    return session;
}

} // namespace opengold::ua
