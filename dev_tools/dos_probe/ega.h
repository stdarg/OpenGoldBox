#ifndef OPENGOLD_DOS_PROBE_EGA_H
#define OPENGOLD_DOS_PROBE_EGA_H

#include <array>
#include <cstdint>
#include <vector>

namespace opengold::probe
{
// The EGA's four 64 KB planes behind the A000 window, with the sequencer and
// graphics controller logic programs drive through ports 3C4-3CF, enough for
// 320 by 200 by 16 colours (mode 0Dh).
class Ega
{
  public:
    Ega();

    std::uint8_t read(std::uint16_t offset);
    void write(std::uint16_t offset, std::uint8_t value);
    // Returns whether the port is the EGA's.
    bool out(std::uint16_t port, std::uint8_t value);
    bool in(std::uint16_t port, std::uint8_t &value);

    void set_palette(unsigned index, std::uint8_t colour)
    {
        attribute_[index & 15U] = colour;
    }

    void set_border(std::uint8_t colour)
    {
        attribute_[17] = colour;
    }

    // The visible 320 by 200 screen as RGB triples, from the CRTC start
    // address, so a page the program is drawing off screen is not shown.
    [[nodiscard]] std::vector<std::uint8_t> rgb_screen() const;

  private:
    std::array<std::vector<std::uint8_t>, 4> planes_;
    std::array<std::uint8_t, 4> latches_{};
    std::array<std::uint8_t, 8> sequencer_{};
    std::array<std::uint8_t, 9> graphics_{};
    std::array<std::uint8_t, 25> crtc_{};
    std::array<std::uint8_t, 21> attribute_{};
    unsigned sequencer_index_{}, graphics_index_{}, crtc_index_{}, attribute_index_{};
    bool attribute_flip_{};
    unsigned status_reads_{};
};
} // namespace opengold::probe

#endif
