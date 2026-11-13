#include "opengold/ua_media.h"
#include "little_endian.h"
#include "ua_detail.h"

#include <algorithm>
#include <string_view>

namespace opengold::ua
{
namespace
{

using detail::Bytes;
using detail::find_chunk;
using detail::has_tag;
using detail::read_big_u16;
using detail::read_big_u32;
using detail::read_chunks;
using detail::unpack_bits;
using format::read_u16;

constexpr std::size_t color_count = 256;

// 8-bit colors as the VGA's DAC takes them, 0 to 63.
VgaPaletteRange read_colors(Bytes rgb)
{
    VgaPaletteRange palette;
    palette.colors.resize(color_count);
    for (std::size_t color = 0; color < color_count; ++color)
    {
        for (std::size_t channel = 0; channel < 3; ++channel)
        {
            palette.colors[color][channel] =
                static_cast<std::uint8_t>(rgb[color * 3 + channel] >> 2);
        }
    }
    return palette;
}

VgaImage blank_image(std::size_t width, std::size_t height)
{
    VgaImage image;
    image.width = static_cast<std::uint16_t>(width);
    image.height = static_cast<std::uint16_t>(height);
    image.pixels.assign(width * height, 0);
    return image;
}

// Each row holds a line of every plane in turn, plane 0 (the low bit) first,
// each line an even number of bytes.
VgaImage interleaved_planes(Bytes body, std::size_t width, std::size_t height, unsigned planes)
{
    const std::size_t line_size = (width + 15) / 16 * 2;
    auto image = blank_image(width, height);
    for (std::size_t row = 0; row < height; ++row)
    {
        for (unsigned plane = 0; plane < planes; ++plane)
        {
            const auto line = body.subspan((row * planes + plane) * line_size, line_size);
            for (std::size_t x = 0; x < width; ++x)
            {
                const auto bit = (line[x / 8] >> (7 - x % 8)) & 1;
                image.pixels[row * width + x] |= static_cast<std::uint8_t>(bit << plane);
            }
        }
    }
    return image;
}

constexpr std::uint32_t microseconds = 1'000'000;

// Creative's 4-bit ADPCM: a first byte that is the starting level, then two
// steps a byte, the high nibble first. A step's bit 3 is its sign and bits 0
// to 2 its size, scaled by a shift that grows after large steps and shrinks
// after zero ones.
std::vector<std::uint8_t> expand_adpcm4(Bytes data)
{
    std::vector<std::uint8_t> samples;
    if (data.empty())
    {
        return samples;
    }
    int level = data[0];
    int shift = 0;
    samples.push_back(static_cast<std::uint8_t>(level));
    auto step = [&](int nibble)
    {
        const int size = nibble & 7;
        const int change = size << shift;
        level = std::clamp((nibble & 8) != 0 ? level - change : level + change, 0, 255);
        if (size >= 5 && shift < 3)
        {
            ++shift;
        }
        else if (size == 0 && shift > 0)
        {
            --shift;
        }
        samples.push_back(static_cast<std::uint8_t>(level));
    };
    for (const auto byte : data.subspan(1))
    {
        step(byte >> 4);
        step(byte & 0x0F);
    }
    return samples;
}

constexpr std::uint8_t voice_terminator = 0;
constexpr std::uint8_t voice_sound = 1;
constexpr std::uint8_t voice_silence = 3;
constexpr std::uint8_t voice_marker = 4;
constexpr std::uint8_t voice_text = 5;
constexpr std::uint8_t pack_8bit = 0;
constexpr std::uint8_t pack_adpcm4 = 1;
constexpr std::uint8_t silent_level = 128;

std::uint32_t voice_rate(std::uint8_t time_constant)
{
    return microseconds / (256 - time_constant);
}

// Standard MIDI's variable-length number: seven bits a byte, high bit set on
// every byte but the last.
std::optional<std::uint32_t> read_variable(Bytes bytes, std::size_t &position)
{
    std::uint32_t value = 0;
    for (int byte_count = 0; byte_count < 4; ++byte_count)
    {
        if (position >= bytes.size())
        {
            return std::nullopt;
        }
        const auto byte = bytes[position++];
        value = value << 7 | (byte & 0x7F);
        if ((byte & 0x80) == 0)
        {
            return value;
        }
    }
    return std::nullopt;
}

// The data bytes a channel event's status takes.
std::size_t channel_data_count(std::uint8_t status)
{
    const auto kind = status & 0xF0;
    return kind == 0xC0 || kind == 0xD0 ? 1 : 2;
}

constexpr std::uint8_t note_on = 0x90;
constexpr std::uint8_t system_exclusive = 0xF0;
constexpr std::uint8_t system_exclusive_end = 0xF7;
constexpr std::uint8_t meta = 0xFF;
constexpr std::uint8_t end_of_track = 0x2F;

// XMI's events: a byte below 80h is a pause of that many ticks (pauses add
// up); there is no running status; a note-on gives its length as a
// variable-length number and has no note-off. The sequence ends with the meta
// event 2Fh.
std::optional<std::vector<XmiEvent>> decode_events(Bytes events)
{
    std::vector<XmiEvent> decoded;
    std::uint32_t tick = 0;
    std::size_t position = 0;
    while (position < events.size())
    {
        const auto status = events[position++];
        if (status < 0x80)
        {
            tick += status;
            continue;
        }
        XmiEvent event;
        event.tick = tick;
        event.status = status;
        if (status == meta)
        {
            if (position >= events.size())
            {
                return std::nullopt;
            }
            event.data1 = events[position++];
        }
        if (status == meta || status == system_exclusive || status == system_exclusive_end)
        {
            const auto length = read_variable(events, position);
            if (!length || *length > events.size() - position)
            {
                return std::nullopt;
            }
            event.payload.assign(events.begin() + static_cast<std::ptrdiff_t>(position),
                                 events.begin() + static_cast<std::ptrdiff_t>(position + *length));
            position += *length;
            decoded.push_back(std::move(event));
            if (status == meta && decoded.back().data1 == end_of_track)
            {
                return decoded;
            }
            continue;
        }
        if (status >= system_exclusive)
        {
            return std::nullopt;
        }
        const auto count = channel_data_count(status);
        if (position + count > events.size() || events[position] >= 0x80 ||
            (count == 2 && events[position + 1] >= 0x80))
        {
            return std::nullopt;
        }
        event.data1 = events[position];
        event.data2 = count == 2 ? events[position + 1] : 0;
        position += count;
        if ((status & 0xF0) == note_on)
        {
            const auto duration = read_variable(events, position);
            if (!duration)
            {
                return std::nullopt;
            }
            event.duration = *duration;
        }
        decoded.push_back(std::move(event));
    }
    return std::nullopt; // No end-of-track event.
}

// A FORM XMID's chunks: TIMB (a count, then a patch and a bank each), RBRN
// (branch points, not needed to play) and EVNT.
std::optional<XmiSequence> decode_sequence(Bytes form)
{
    if (!has_tag(form, 0, "XMID"))
    {
        return std::nullopt;
    }
    const auto chunks = read_chunks(form, 4);
    if (!chunks)
    {
        return std::nullopt;
    }
    XmiSequence sequence;
    if (const auto *timbres = find_chunk(*chunks, "TIMB"))
    {
        const auto &data = timbres->data;
        if (data.size() < 2 || data.size() < 2 + std::size_t{read_u16(data, 0)} * 2)
        {
            return std::nullopt;
        }
        for (std::size_t timbre = 0; timbre < read_u16(data, 0); ++timbre)
        {
            sequence.timbres.push_back({data[2 + timbre * 2], data[3 + timbre * 2]});
        }
    }
    const auto *events = find_chunk(*chunks, "EVNT");
    if (events == nullptr)
    {
        return std::nullopt;
    }
    auto decoded = decode_events(events->data);
    if (!decoded)
    {
        return std::nullopt;
    }
    sequence.events = std::move(*decoded);
    return sequence;
}

} // namespace

std::optional<ScreenPicture> decode_ilbm(std::span<const std::uint8_t> bytes)
{
    constexpr std::size_t header_size = 20;
    constexpr unsigned plane_count = 8;
    if (!has_tag(bytes, 0, "FORM") || !has_tag(bytes, 8, "ILBM") || bytes.size() < 12)
    {
        return std::nullopt;
    }
    const auto chunks = read_chunks(
        bytes.first(std::min<std::size_t>(bytes.size(), 8 + std::size_t{read_big_u32(bytes, 4)})),
        12);
    if (!chunks)
    {
        return std::nullopt;
    }
    const auto *header = find_chunk(*chunks, "BMHD");
    const auto *colors = find_chunk(*chunks, "CMAP");
    const auto *body = find_chunk(*chunks, "BODY");
    if (header == nullptr || colors == nullptr || body == nullptr ||
        header->data.size() < header_size || colors->data.size() < color_count * 3)
    {
        return std::nullopt;
    }
    const std::size_t width = read_big_u16(header->data, 0);
    const std::size_t height = read_big_u16(header->data, 2);
    const auto planes = header->data[8];
    const auto masking = header->data[9];
    const auto compression = header->data[10];
    // The kit writes 8 planes with no mask plane.
    if (width == 0 || height == 0 || planes != plane_count || masking == 1 || compression > 1)
    {
        return std::nullopt;
    }
    const std::size_t body_size = (width + 15) / 16 * 2 * plane_count * height;
    std::vector<std::uint8_t> unpacked;
    if (compression == 0)
    {
        if (body->data.size() < body_size)
        {
            return std::nullopt;
        }
        unpacked.assign(body->data.begin(),
                        body->data.begin() + static_cast<std::ptrdiff_t>(body_size));
    }
    else
    {
        std::size_t position = 0;
        if (!unpack_bits(body->data, position, body_size, unpacked))
        {
            return std::nullopt;
        }
    }
    ScreenPicture picture;
    picture.image = interleaved_planes(unpacked, width, height, plane_count);
    picture.palette = read_colors(colors->data);
    return picture;
}

std::optional<ScreenPicture> decode_pcx(std::span<const std::uint8_t> bytes)
{
    constexpr std::size_t header_size = 128;
    constexpr std::size_t palette_size = 1 + color_count * 3;
    constexpr std::uint8_t palette_mark = 0x0C;
    if (bytes.size() < header_size + palette_size || bytes[0] != 0x0A || bytes[1] != 5 ||
        bytes[2] != 1 || bytes[3] != 8 || bytes[65] != 1 ||
        bytes[bytes.size() - palette_size] != palette_mark)
    {
        return std::nullopt;
    }
    const auto first_x = read_u16(bytes, 4);
    const auto first_y = read_u16(bytes, 6);
    const auto last_x = read_u16(bytes, 8);
    const auto last_y = read_u16(bytes, 10);
    const std::size_t line_size = read_u16(bytes, 66);
    if (last_x < first_x || last_y < first_y || line_size < std::size_t(last_x - first_x + 1))
    {
        return std::nullopt;
    }
    const std::size_t width = last_x - first_x + 1;
    const std::size_t height = last_y - first_y + 1;
    // A byte of C0h or more repeats the next byte (its low six bits) times.
    const auto data = bytes.subspan(header_size, bytes.size() - header_size - palette_size);
    std::vector<std::uint8_t> lines;
    const std::size_t total = line_size * height;
    std::size_t position = 0;
    while (lines.size() < total)
    {
        if (position >= data.size())
        {
            return std::nullopt;
        }
        const auto byte = data[position++];
        if (byte < 0xC0)
        {
            lines.push_back(byte);
            continue;
        }
        if (position >= data.size())
        {
            return std::nullopt;
        }
        lines.insert(lines.end(), byte & 0x3F, data[position++]);
    }
    ScreenPicture picture;
    picture.image = blank_image(width, height);
    for (std::size_t row = 0; row < height; ++row)
    {
        std::copy_n(lines.begin() + static_cast<std::ptrdiff_t>(row * line_size), width,
                    picture.image.pixels.begin() + static_cast<std::ptrdiff_t>(row * width));
    }
    picture.palette = read_colors(bytes.last(palette_size - 1));
    return picture;
}

std::optional<std::vector<VoiceSection>> decode_voice_file(std::span<const std::uint8_t> bytes)
{
    constexpr std::string_view signature = "Creative Voice File\x1A";
    if (!has_tag(bytes, 0, signature) || bytes.size() < signature.size() + 6)
    {
        return std::nullopt;
    }
    const std::size_t first_block = read_u16(bytes, 20);
    const auto version = read_u16(bytes, 22);
    if (read_u16(bytes, 24) != static_cast<std::uint16_t>(~version + 0x1234) ||
        first_block < signature.size() + 6)
    {
        return std::nullopt;
    }
    std::vector<VoiceSection> sections;
    auto current = [&sections]() -> VoiceSection &
    {
        if (sections.empty())
        {
            sections.emplace_back();
        }
        return sections.back();
    };
    std::size_t position = first_block;
    while (position < bytes.size())
    {
        const auto type = bytes[position];
        if (type == voice_terminator)
        {
            return sections;
        }
        if (position + 4 > bytes.size())
        {
            return std::nullopt;
        }
        const std::size_t size =
            bytes[position + 1] | bytes[position + 2] << 8 | bytes[position + 3] << 16;
        if (size > bytes.size() - position - 4)
        {
            return std::nullopt;
        }
        const auto data = bytes.subspan(position + 4, size);
        position += 4 + size;
        switch (type)
        {
        case voice_sound:
        {
            if (data.size() < 2 || (data[1] != pack_8bit && data[1] != pack_adpcm4))
            {
                return std::nullopt;
            }
            auto &section = current();
            section.rate = voice_rate(data[0]);
            const auto sound = data.subspan(2);
            if (data[1] == pack_8bit)
            {
                section.samples.insert(section.samples.end(), sound.begin(), sound.end());
            }
            else
            {
                const auto expanded = expand_adpcm4(sound);
                section.samples.insert(section.samples.end(), expanded.begin(), expanded.end());
            }
            break;
        }
        case voice_silence:
        {
            if (data.size() != 3)
            {
                return std::nullopt;
            }
            auto &section = current();
            section.rate = voice_rate(data[2]);
            section.samples.insert(section.samples.end(), std::size_t{read_u16(data, 0)} + 1,
                                   silent_level);
            break;
        }
        case voice_marker:
            if (data.size() != 2)
            {
                return std::nullopt;
            }
            sections.push_back({read_u16(data, 0), 0, {}});
            break;
        case voice_text:
            break;
        default:
            return std::nullopt;
        }
    }
    return std::nullopt; // No terminator.
}

std::optional<std::vector<XmiSequence>> decode_xmi(std::span<const std::uint8_t> bytes)
{
    const auto top = read_chunks(bytes, 0);
    if (!top || top->empty())
    {
        return std::nullopt;
    }
    // A single sequence may stand alone as FORM XMID.
    if ((*top)[0].tag == "FORM" && has_tag((*top)[0].data, 0, "XMID"))
    {
        auto sequence = decode_sequence((*top)[0].data);
        return sequence ? std::optional(std::vector{std::move(*sequence)}) : std::nullopt;
    }
    // Otherwise FORM XDIR, whose INFO counts the sequences, then CAT XMID.
    if (top->size() != 2 || (*top)[0].tag != "FORM" || !has_tag((*top)[0].data, 0, "XDIR") ||
        (*top)[1].tag != "CAT " || !has_tag((*top)[1].data, 0, "XMID"))
    {
        return std::nullopt;
    }
    const auto directory = read_chunks((*top)[0].data, 4);
    const auto *info = directory ? find_chunk(*directory, "INFO") : nullptr;
    const auto forms = read_chunks((*top)[1].data, 4);
    if (info == nullptr || info->data.size() < 2 || !forms ||
        forms->size() != read_u16(info->data, 0))
    {
        return std::nullopt;
    }
    std::vector<XmiSequence> sequences;
    for (const auto &form : *forms)
    {
        auto sequence = form.tag == "FORM" ? decode_sequence(form.data) : std::nullopt;
        if (!sequence)
        {
            return std::nullopt;
        }
        sequences.push_back(std::move(*sequence));
    }
    return sequences;
}

} // namespace opengold::ua
