#include "opengold/vace_film.h"

#include "little_endian.h"

#include <algorithm>
#include <utility>

namespace opengold
{
namespace
{

using format::read_u16;
using format::read_u32;
using Bytes = std::span<const std::uint8_t>;

// The control file's commands that shape the picture, start the song and let a
// key end the film (docs/games/darkness.md, "The opening and ending films",
// "The control file"). The others stop the music or do nothing.
enum class Command : std::uint8_t
{
    set_pace = 0x00,
    end = 0x01,
    set_color = 0x02,
    draw_next_frame = 0x03,
    list_block = 0x05,
    load_song = 0x08,
    start_song = 0x09,
    clear_screen = 0x0b,
    keys_end_film = 0x0d,
    keys_ignored = 0x0e,
};

// One command: its number and the 16-bit words after it.
struct ControlCommand
{
    Command command{};
    std::vector<std::uint16_t> arguments;
};

// A block of frames in the .BMA and where each frame starts inside it.
struct FrameBlock
{
    Bytes bytes;
    std::vector<std::uint32_t> frame_offsets;
};

// The commands up to the end command, or nullopt when one runs past the file.
std::optional<std::vector<ControlCommand>> parse_commands(Bytes control)
{
    std::vector<ControlCommand> commands;
    std::size_t at = 0;
    while (at + 2 <= control.size())
    {
        const auto length = control[at];
        if (length < 2 || length % 2 != 0 || at + length > control.size())
        {
            return std::nullopt;
        }
        ControlCommand command{static_cast<Command>(control[at + 1]), {}};
        for (std::size_t word = at + 2; word < at + length; word += 2)
        {
            command.arguments.push_back(read_u16(control, word));
        }
        at += length;
        if (command.command == Command::end)
        {
            return commands;
        }
        commands.push_back(std::move(command));
    }
    return at == control.size() ? std::optional(std::move(commands)) : std::nullopt;
}

// Each block: its size (4 bytes, the header included), its frame count (2),
// then where each frame starts (4 bytes each, from the block's start).
std::optional<std::vector<FrameBlock>> parse_blocks(Bytes frames)
{
    std::vector<FrameBlock> blocks;
    std::size_t at = 0;
    while (at < frames.size())
    {
        if (frames.size() - at < 6)
        {
            return std::nullopt;
        }
        const auto block_size = read_u32(frames, at);
        const auto frame_count = read_u16(frames, at + 4);
        const auto header_size = 6 + std::size_t{frame_count} * 4;
        if (block_size < header_size || block_size > frames.size() - at)
        {
            return std::nullopt;
        }
        FrameBlock block{frames.subspan(at, block_size), {}};
        for (std::size_t frame = 0; frame < frame_count; ++frame)
        {
            const auto offset = read_u32(block.bytes, 6 + frame * 4);
            if (offset < header_size || offset > block_size - 4)
            {
                return std::nullopt;
            }
            block.frame_offsets.push_back(offset);
        }
        blocks.push_back(std::move(block));
        at += block_size;
    }
    return blocks;
}

// The control file lists every block in order: its number, its frame count
// and the low 16 bits of its size.
bool blocks_match_their_list(const std::vector<ControlCommand> &commands,
                             const std::vector<FrameBlock> &blocks)
{
    std::size_t listed = 0;
    for (const auto &command : commands)
    {
        if (command.command != Command::list_block)
        {
            continue;
        }
        if (command.arguments.size() < 3 || listed >= blocks.size())
        {
            return false;
        }
        const auto &block = blocks[listed];
        if (command.arguments[0] != listed || command.arguments[1] != block.frame_offsets.size() ||
            command.arguments[2] != (block.bytes.size() & 0xFFFF))
        {
            return false;
        }
        ++listed;
    }
    return listed == blocks.size();
}

// One run of a row: count + 1 pixels, packed in the low bit. A set bit repeats
// the next byte, a clear one copies the next count + 1 bytes. Returns the byte
// after the run, or nullopt when it leaves the data or the row.
std::optional<std::size_t> draw_run(Bytes data, std::size_t at, std::span<std::uint8_t> row,
                                    std::size_t &column)
{
    const auto code = data[at++];
    const std::size_t count = (code >> 1) + 1;
    const bool repeats = (code & 1) != 0;
    const auto source_size = repeats ? std::size_t{1} : count;
    if (column + count > row.size() || source_size > data.size() - at)
    {
        return std::nullopt;
    }
    for (std::size_t pixel = 0; pixel < count; ++pixel)
    {
        row[column + pixel] = data[at + (repeats ? 0 : pixel)];
    }
    column += count;
    return at + source_size;
}

// A frame: its width and height (2 bytes each), then the rows it changes. Each
// starts with its row number (0xFF ends the frame), then one or more pieces:
// the first column (the top bit marks the row's last piece), the pixel count,
// the size of the runs, and the runs. Pixels the frame leaves out keep the
// screen's color.
bool draw_frame(const FrameBlock &block, std::uint32_t offset, std::vector<std::uint8_t> &screen)
{
    const auto bytes = block.bytes;
    if (read_u16(bytes, offset) != vace_film_width ||
        read_u16(bytes, offset + 2) != vace_film_height)
    {
        return false;
    }
    std::size_t at = offset + 4;
    while (true)
    {
        if (at >= bytes.size())
        {
            return false;
        }
        const auto row_number = bytes[at++];
        if (row_number == 0xFF)
        {
            return true;
        }
        if (row_number >= vace_film_height)
        {
            return false;
        }
        const auto row =
            std::span(screen).subspan(std::size_t{row_number} * vace_film_width, vace_film_width);
        bool last_piece = false;
        while (!last_piece)
        {
            if (bytes.size() - at < 4)
            {
                return false;
            }
            const auto first_column = read_u16(bytes, at);
            last_piece = (first_column & 0x8000) != 0;
            const std::size_t pixel_count = bytes[at + 2];
            const std::size_t data_size = bytes[at + 3];
            at += 4;
            if (data_size > bytes.size() - at)
            {
                return false;
            }
            const auto data = bytes.subspan(at, data_size);
            std::size_t column = first_column & 0x7FFF;
            const auto start_column = column;
            std::size_t data_at = 0;
            while (data_at < data.size())
            {
                const auto next = draw_run(data, data_at, row, column);
                if (!next)
                {
                    return false;
                }
                data_at = *next;
            }
            if (column - start_column != pixel_count)
            {
                return false;
            }
            at += data_size;
        }
    }
}

// Every frame of every block, in the order the film draws them.
std::vector<std::pair<const FrameBlock *, std::uint32_t>>
frames_in_order(const std::vector<FrameBlock> &blocks)
{
    std::vector<std::pair<const FrameBlock *, std::uint32_t>> order;
    for (const auto &block : blocks)
    {
        for (const auto offset : block.frame_offsets)
        {
            order.emplace_back(&block, offset);
        }
    }
    return order;
}

// VACE counts the ticks of the PC's timer, which the sound driver sets to
// 1,193,182 / 16,384 (about 72.8) a second, and holds each frame for
// 261 / pace of them.
std::uint32_t hold_milliseconds(std::uint16_t pace)
{
    const std::uint32_t ticks = 261 / pace;
    return static_cast<std::uint32_t>(std::uint64_t{ticks} * 16'384 * 1'000 / 1'193'182);
}

// The load-song command's words: the song's number in the first word's low
// byte, then the file's name two letters a word, high byte first, up to a
// space; VACE adds ".SMS". nullopt when the command is short.
std::optional<VaceSong> song_to_load(const std::vector<std::uint16_t> &arguments)
{
    if (arguments.size() < 4)
    {
        return std::nullopt;
    }
    std::string name;
    for (std::size_t word = 1; word < 4; ++word)
    {
        name.push_back(static_cast<char>(arguments[word] >> 8));
        name.push_back(static_cast<char>(arguments[word] & 0xFF));
    }
    name.resize(std::min(name.find(' '), name.size()));
    return VaceSong{name + ".SMS", static_cast<std::uint8_t>(arguments[0] & 0xFF)};
}

} // namespace

std::optional<std::vector<VaceFilmFrame>> decode_vace_film(std::span<const std::uint8_t> control,
                                                           std::span<const std::uint8_t> frames)
{
    const auto commands = parse_commands(control);
    const auto blocks = parse_blocks(frames);
    if (!commands || !blocks || !blocks_match_their_list(*commands, *blocks))
    {
        return std::nullopt;
    }
    const auto frame_order = frames_in_order(*blocks);
    std::vector<VaceFilmFrame> film;
    VaceFilmFrame screen{std::vector<std::uint8_t>(vace_film_width * vace_film_height), {}, 0};
    std::uint16_t pace = 0;
    bool keys_end_film = false;
    std::optional<VaceSong> loaded_song;
    // The song started since the last frame was drawn.
    std::optional<VaceSong> started_song;
    // The frame on screen whose time runs out at the next draw or clear.
    std::optional<std::size_t> showing;
    const auto end_showing = [&]
    {
        if (!showing)
        {
            return true;
        }
        if (pace == 0)
        {
            return false;
        }
        film[*showing].hold_milliseconds = hold_milliseconds(pace);
        showing.reset();
        return true;
    };
    for (const auto &[command, arguments] : *commands)
    {
        switch (command)
        {
        case Command::set_pace:
            if (arguments.empty() || arguments[0] == 0)
            {
                return std::nullopt;
            }
            pace = arguments[0];
            break;
        case Command::set_color:
        {
            if (arguments.size() < 2)
            {
                return std::nullopt;
            }
            auto &color = screen.palette[arguments[0] >> 8];
            color = {static_cast<std::uint8_t>(arguments[0] & 0xFF),
                     static_cast<std::uint8_t>(arguments[1] >> 8),
                     static_cast<std::uint8_t>(arguments[1] & 0xFF)};
            break;
        }
        case Command::draw_next_frame:
        {
            if (film.size() >= frame_order.size() || !end_showing())
            {
                return std::nullopt;
            }
            const auto &[block, offset] = frame_order[film.size()];
            if (!draw_frame(*block, offset, screen.pixels))
            {
                return std::nullopt;
            }
            showing = film.size();
            film.push_back(screen);
            film.back().key_ends_film = keys_end_film;
            film.back().starts_song = std::exchange(started_song, std::nullopt);
            break;
        }
        case Command::load_song:
            loaded_song = song_to_load(arguments);
            if (!loaded_song)
            {
                return std::nullopt;
            }
            break;
        case Command::start_song:
            started_song = loaded_song;
            break;
        case Command::keys_end_film:
            keys_end_film = true;
            break;
        case Command::keys_ignored:
            keys_end_film = false;
            break;
        case Command::clear_screen:
            if (!end_showing())
            {
                return std::nullopt;
            }
            std::fill(screen.pixels.begin(), screen.pixels.end(), std::uint8_t{0});
            break;
        default:
            break;
        }
    }
    if (!end_showing())
    {
        return std::nullopt;
    }
    return film;
}

} // namespace opengold
