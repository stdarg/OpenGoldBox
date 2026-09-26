#ifndef OPENGOLD_ECL_H
#define OPENGOLD_ECL_H
#include <array>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace opengold::por
{
class EclError : public std::runtime_error
{
  public:
    using std::runtime_error::runtime_error;
};
struct EclOperand
{
    std::uint8_t tag{};
    std::uint16_t value{}; // Encoded value/address; dereferencing depends on opcode role.
    std::string text;
};
struct EclInstruction
{
    std::uint16_t address{};
    std::uint32_t next{};
    std::uint8_t opcode{};
    std::vector<EclOperand> operands;
};
struct EclOpcode
{
    std::string_view name;
    unsigned operands{};
    bool variable_list{}, executable{}, requires_host{};
};
[[nodiscard]] const EclOpcode &ecl_opcode(std::uint8_t opcode);
[[nodiscard]] std::string unpack_ecl_text(std::span<const std::uint8_t> bytes);

class EclProgram
{
  public:
    static constexpr std::uint32_t origin = 0x9900;
    static constexpr std::uint32_t limit = 0xB700; // PoR zone 3 ends at B6FF.
    // Validates record size and five entry jumps. Reachable bodies decode on demand:
    // embedded data is not interpreted as instructions. Prefix is retained, not guessed.
    [[nodiscard]] static EclProgram decode(std::span<const std::uint8_t> record,
                                           std::string source);
    [[nodiscard]] EclInstruction instruction(std::uint32_t address) const;
    // Decode a machine's private, potentially modified copy of this record.
    [[nodiscard]] EclInstruction instruction(std::uint32_t address,
                                             std::span<const std::uint8_t> image) const;
    [[nodiscard]] const std::array<std::uint16_t, 5> &entries() const noexcept
    {
        return entries_;
    }
    [[nodiscard]] const std::string &source() const noexcept
    {
        return source_;
    }
    [[nodiscard]] const std::vector<std::uint8_t> &raw() const noexcept
    {
        return raw_;
    }
    [[nodiscard]] std::uint32_t body_start() const noexcept
    {
        return body_start_;
    }

  private:
    EclProgram() = default;
    std::vector<std::uint8_t> raw_;
    std::string source_;
    std::array<std::uint16_t, 5> entries_{};
    std::uint32_t body_start_{};
};
} // namespace opengold::por
#endif
