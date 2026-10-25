#include "ega.h"

namespace opengold::probe
{
Ega::Ega()
{
    for (auto &plane : planes_)
    {
        plane.assign(65536, 0);
    }
    sequencer_[2] = 0x0F;
    graphics_[8] = 0xFF;
    graphics_[7] = 0x0F;
    for (unsigned n = 0; n < 16; ++n)
    {
        attribute_[n] = static_cast<std::uint8_t>(n < 8 ? n : n + 8);
    }
}

std::uint8_t Ega::read(std::uint16_t offset)
{
    for (unsigned p = 0; p < 4; ++p)
    {
        latches_[p] = planes_[p][offset];
    }
    if ((graphics_[5] & 0x08U) == 0)
    {
        return latches_[graphics_[4] & 3U];
    }
    // Read mode 1: a bit is set where the pixel matches the compare colour.
    std::uint8_t result = 0;
    for (unsigned bit = 0; bit < 8; ++bit)
    {
        unsigned colour = 0;
        for (unsigned p = 0; p < 4; ++p)
        {
            colour |= ((latches_[p] >> bit) & 1U) << p;
        }
        if (((colour ^ graphics_[2]) & graphics_[7] & 0x0FU) == 0)
        {
            result = static_cast<std::uint8_t>(result | (1U << bit));
        }
    }
    return result;
}

void Ega::write(std::uint16_t offset, std::uint8_t value)
{
    const unsigned mode = graphics_[5] & 3U;
    const unsigned rotation = graphics_[3] & 7U;
    const unsigned function = (graphics_[3] >> 3U) & 3U;
    const std::uint8_t mask = graphics_[8];
    const std::uint8_t rotated =
        static_cast<std::uint8_t>((value >> rotation) | (value << ((8U - rotation) & 7U)));
    for (unsigned p = 0; p < 4; ++p)
    {
        if ((sequencer_[2] & (1U << p)) == 0)
        {
            continue;
        }
        if (mode == 1)
        {
            planes_[p][offset] = latches_[p];
            continue;
        }
        std::uint8_t data = 0;
        if (mode == 2)
        {
            data = (value & (1U << p)) ? 0xFF : 0x00;
        }
        else if (graphics_[1] & (1U << p))
        {
            data = (graphics_[0] & (1U << p)) ? 0xFF : 0x00;
        }
        else
        {
            data = rotated;
        }
        switch (function)
        {
        case 1:
            data &= latches_[p];
            break;
        case 2:
            data |= latches_[p];
            break;
        case 3:
            data ^= latches_[p];
            break;
        default:
            break;
        }
        planes_[p][offset] = static_cast<std::uint8_t>((data & mask) | (latches_[p] & ~mask));
    }
}

bool Ega::out(std::uint16_t port, std::uint8_t value)
{
    switch (port)
    {
    case 0x3C0:
        if (!attribute_flip_)
        {
            attribute_index_ = value & 0x1FU;
        }
        else if (attribute_index_ < attribute_.size())
        {
            attribute_[attribute_index_] = value;
        }
        attribute_flip_ = !attribute_flip_;
        return true;
    case 0x3C4:
        sequencer_index_ = value & 7U;
        return true;
    case 0x3C5:
        sequencer_[sequencer_index_] = value;
        return true;
    case 0x3CE:
        graphics_index_ = value & 0x0FU;
        return true;
    case 0x3CF:
        if (graphics_index_ < graphics_.size())
        {
            graphics_[graphics_index_] = value;
        }
        return true;
    case 0x3D4:
        crtc_index_ = value & 0x1FU;
        return true;
    case 0x3D5:
        if (crtc_index_ < crtc_.size())
        {
            crtc_[crtc_index_] = value;
        }
        return true;
    default:
        return port >= 0x3C0 && port <= 0x3DF;
    }
}

bool Ega::in(std::uint16_t port, std::uint8_t &value)
{
    switch (port)
    {
    case 0x3DA:
        // Alternates display enable and vertical retrace so wait loops end.
        attribute_flip_ = false;
        ++status_reads_;
        value = (status_reads_ & 1U) ? 0x09 : 0x00;
        return true;
    case 0x3C5:
        value = sequencer_[sequencer_index_];
        return true;
    case 0x3CF:
        value = graphics_index_ < graphics_.size() ? graphics_[graphics_index_] : 0;
        return true;
    case 0x3D5:
        value = crtc_index_ < crtc_.size() ? crtc_[crtc_index_] : 0;
        return true;
    default:
        value = 0xFF;
        return port >= 0x3C0 && port <= 0x3DF;
    }
}

std::vector<std::uint8_t> Ega::rgb_screen() const
{
    // In 200-line modes the monitor reads palette bit 4 as intensity; colour 6
    // is drawn brown, as on the CGA palette these modes copy.
    static constexpr std::array<std::array<std::uint8_t, 3>, 16> colours{{{0, 0, 0},
                                                                          {0, 0, 170},
                                                                          {0, 170, 0},
                                                                          {0, 170, 170},
                                                                          {170, 0, 0},
                                                                          {170, 0, 170},
                                                                          {170, 85, 0},
                                                                          {170, 170, 170},
                                                                          {85, 85, 85},
                                                                          {85, 85, 255},
                                                                          {85, 255, 85},
                                                                          {85, 255, 255},
                                                                          {255, 85, 85},
                                                                          {255, 85, 255},
                                                                          {255, 255, 85},
                                                                          {255, 255, 255}}};
    const unsigned start = (unsigned{crtc_[12]} << 8U) | crtc_[13];
    std::vector<std::uint8_t> rgb;
    rgb.reserve(320 * 200 * 3);
    for (unsigned y = 0; y < 200; ++y)
    {
        for (unsigned x = 0; x < 320; ++x)
        {
            const auto offset = static_cast<std::uint16_t>(start + y * 40 + x / 8);
            const unsigned bit = 7 - (x & 7U);
            unsigned index = 0;
            for (unsigned p = 0; p < 4; ++p)
            {
                index |= ((planes_[p][offset] >> bit) & 1U) << p;
            }
            const unsigned value = attribute_[index];
            const unsigned colour = (value & 7U) | ((value & 0x10U) ? 8U : 0U);
            for (const auto channel : colours[colour])
            {
                rgb.push_back(channel);
            }
        }
    }
    return rgb;
}
} // namespace opengold::probe
