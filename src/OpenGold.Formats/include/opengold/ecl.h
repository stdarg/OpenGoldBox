#ifndef OPENGOLD_ECL_H
#define OPENGOLD_ECL_H
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "opengold/gold_box_game.h"

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

// A game's opcode; throws EclError for an opcode the game does not have, or a
// game whose scripts are not decoded yet.
[[nodiscard]] const EclOpcode &ecl_opcode(std::uint8_t opcode,
                                          GoldBoxGame game = GoldBoxGame::pool_of_radiance);

// Where a game's scripts run in the script machine's memory: the first address
// of the script bank and the address past its end.
struct EclMemory
{
    std::uint32_t origin{}, limit{};
};

// Throws EclError for a game whose scripts are not decoded yet.
[[nodiscard]] EclMemory ecl_memory(GoldBoxGame game);

// The original engine routines a script's CALL may run, by the address it
// names in each game. Any other address does nothing.
struct EclEngineRoutines
{
    // Redraw the 3D view from the party's square; both redraw it in Pool of
    // Radiance, while Curse's second only reads the wall ahead.
    std::uint16_t redraw_view{}, refresh_wall_ahead{};
    std::uint16_t move_forward{};
    // The selected member fights alone; in the mirror duel, against a copy of itself.
    std::uint16_t duel{}, mirror_duel{};
    // Play the sound the selector cell 3DEh names.
    std::uint16_t sound{};
    // Show the portrait animation's next frame; Pool of Radiance has none (0).
    std::uint16_t portrait_animation_step{};
    // Gateway to the Savage Frontier's prompts: wait for Enter, and ask Yes or
    // No into the menu choice cell 7F79h and the comparison (0 elsewhere).
    // The machine runs them as menus; contains() leaves them out.
    std::uint16_t wait_for_enter{}, ask_yes_no{};
    // Gateway's change of the current character's coins up into larger ones
    // (0 elsewhere).
    std::uint16_t change_coins_up{};

    [[nodiscard]] bool contains(std::uint16_t address) const noexcept
    {
        return address != 0 && (address == redraw_view || address == refresh_wall_ahead ||
                                address == move_forward || address == duel ||
                                address == mirror_duel || address == sound ||
                                address == portrait_animation_step || address == change_coins_up);
    }
};

// Throws EclError for a game whose scripts are not decoded yet.
[[nodiscard]] EclEngineRoutines ecl_engine_routines(GoldBoxGame game);
[[nodiscard]] std::string unpack_ecl_text(std::span<const std::uint8_t> bytes);

// A literal operand an instruction reads in place of the one its record
// holds, as data/encounters/chances.json changes a script's encounter chance.
struct EclOperandOverride
{
    std::uint16_t address{};
    std::uint8_t operand{};
    std::uint16_t value{};
};

class EclProgram
{
  public:
    // Pool of Radiance's script bank (ecl_memory); the script machine runs only its scripts.
    static constexpr std::uint32_t origin = 0x9900;
    static constexpr std::uint32_t limit = 0xB700; // PoR zone 3 ends at B6FF.
    // Validates record size and five entry jumps. Reachable bodies decode on demand:
    // embedded data is not interpreted as instructions. Prefix is retained, not guessed.
    [[nodiscard]] static EclProgram decode(std::span<const std::uint8_t> record,
                                           std::string_view source,
                                           GoldBoxGame game = GoldBoxGame::pool_of_radiance);
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

    [[nodiscard]] GoldBoxGame game() const noexcept
    {
        return game_;
    }

    [[nodiscard]] EclMemory memory() const noexcept
    {
        return memory_;
    }

    // This program with those operands read in place of its record's. The
    // record and a running script's image keep the original bytes, so a save
    // holds none of them.
    [[nodiscard]] EclProgram
    with_operand_overrides(std::vector<EclOperandOverride> overrides) const;

  private:
    EclProgram() = default;
    std::vector<EclOperandOverride> overrides_;
    std::vector<std::uint8_t> raw_;
    std::string source_;
    std::array<std::uint16_t, 5> entries_{};
    std::uint32_t body_start_{};
    GoldBoxGame game_{GoldBoxGame::pool_of_radiance};
    EclMemory memory_{origin, limit};
};

struct EclWalkError
{
    std::uint32_t address{};
    // Set when the address was only reached by falling through this ON GOTO.
    std::optional<std::uint16_t> fallthrough_from;
    std::string message;
};

struct EclWalk
{
    std::vector<EclInstruction> instructions; // In visit order.
    std::vector<EclWalkError> errors;
};

// Every instruction reachable from the entries through jumps, calls, ON GOTO/GOSUB
// targets and IF skips, without running the script.
[[nodiscard]] EclWalk walk_reachable(const EclProgram &program);
} // namespace opengold::por
#endif
