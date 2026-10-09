#include "../src/OpenGoldBox/monster_picture_timing.h"
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

// The monster close-up before a fight. The view keeps its own copy of the
// frame delays; the session's frames go when the session is reset or replaced.
namespace
{
void check(bool ok, const std::string &message)
{
    if (!ok)
        throw std::runtime_error(message);
}

constexpr double tick = 1.0 / 18.2;

void frames_follow_the_original_timer()
{
    // An orc-like record: two held poses with a zero-tick frame between them.
    const std::array<std::uint32_t, 3> delays{4, 0, 6};
    check(presentation::monster_frame_at(delays, 0) == 0, "The first pose shows first");
    check(presentation::monster_frame_at(delays, 3.5 * tick) == 0, "It holds for its four ticks");
    check(presentation::monster_frame_at(delays, 4.5 * tick) == 2,
          "A zero-tick frame is passed over");
    check(presentation::monster_frame_at(delays, 10.5 * tick) == 0, "The close-up loops");
    const std::array<std::uint32_t, 2> still{0, 0};
    check(presentation::monster_frame_at(still, 5) == 0, "A picture with no timing stays put");
}

// shown_monster_picture_ points into the session, so the view may only compare
// it; reading frames through it would use a destroyed session's data.
void view_never_reads_through_the_session_pointer()
{
    const auto path = std::filesystem::path(OPENGOLD_SOURCE_DIR) / "src/OpenGoldBox/rolf_tour_view.cpp";
    std::ifstream in(path);
    const std::string text{std::istreambuf_iterator<char>(in), {}};
    check(!text.empty(), "The exploration view's source is found");
    for (const char *read : {"*shown_monster_picture_", "shown_monster_picture_->",
                             "shown_monster_picture_)["})
        check(text.find(read) == std::string::npos,
              std::string("rolf_tour_view.cpp reads the close-up through the session: ") + read);
}
} // namespace

int main()
{
    try
    {
        frames_follow_the_original_timer();
        view_never_reads_through_the_session_pointer();
        std::cout << "Monster picture checks passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
