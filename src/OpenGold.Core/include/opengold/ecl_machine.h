#ifndef OPENGOLD_ECL_MACHINE_H
#define OPENGOLD_ECL_MACHINE_H
#include "opengold/ecl.h"
#include <filesystem>
#include <compare>
#include <map>
#include <memory>
#include <optional>
#include <random>
#include <set>

namespace opengold
{
struct SaveCodec;
}

namespace opengold::por
{
struct ScriptId
{
    std::string archive;
    std::uint8_t record{};
    auto operator<=>(const ScriptId &) const = default;
};

class EclCatalog
{
  public:
    [[nodiscard]] static EclCatalog load(const std::filesystem::path &directory);

    [[nodiscard]] const auto &all() const noexcept
    {
        return programs_;
    }

    [[nodiscard]] std::shared_ptr<const EclProgram> find(const ScriptId &id) const;

  private:
    std::map<ScriptId, std::shared_ptr<const EclProgram>> programs_;
};

enum class EclState
{
    idle,
    running,
    waiting,
    completed,
    faulted
};
enum class EclRequestKind
{
    text,
    menu,
    input_number,
    input_string,
    host
};
enum class EclArgumentKind
{
    number,
    address,
    text
};

struct EclHostArgument
{
    EclArgumentKind kind{};
    std::uint16_t value{};
    std::string text;
};

using EclConditions = std::array<bool, 6>; // =, <>, <, >, <=, >=

struct EclMemoryWrite
{
    std::uint16_t address{}, value{};
};

struct EclHostReply
{
    std::vector<EclMemoryWrite> writes;
    // FIND ITEM must return its condition flags; other host operations preserve them.
    std::optional<EclConditions> conditions;
    // Required only for NEW ECL. The host resolves the current disk/script ID.
    std::shared_ptr<const EclProgram> next_program;
};

struct EclRequest
{
    std::uint64_t id{};
    EclRequestKind kind{};
    std::string text; // Text to display, or vertical-menu header/delay text operand.
    bool clear{}, vertical{};
    std::vector<std::string> choices;
    std::size_t input_limit{};
    std::optional<EclInstruction> instruction; // Original encoded operands and source PC.
    std::vector<EclHostArgument> arguments;    // Resolved numbers/text, encoded addresses.
};

struct EclRunResult
{
    EclState state{};
    std::size_t instructions{}; // Running means the caller's instruction budget was exhausted.
    std::optional<EclRequest> request;
    std::string diagnostic;
};

// PoR VM with private writable script bytes and explicitly bound logical variables.
// Engine-dependent operations require opt-in host capabilities, never pretend to
// run combat or DOS machine code. See docs/SCRIPTS.md for compatibility limits.
class EclMachine
{
  public:
    explicit EclMachine(std::shared_ptr<const EclProgram> program);
    // Variables persist across invocations; binding is allowed only while idle/completed.
    void bind_variable(std::uint16_t address, std::uint16_t value);
    void bind_string(std::uint16_t address, std::string_view value);
    void seed_random(std::uint32_t seed);
    void enable_host(std::uint8_t opcode);
    [[nodiscard]] std::uint16_t variable(std::uint16_t address) const;
    [[nodiscard]] std::string string(std::uint16_t address) const;
    bool start(std::size_t entry_slot);
    // Explicit isolated research entry. Bypasses campaign entry scheduling; the
    // caller must identify an instruction boundary from its compatibility profile.
    bool start_at_for_inspection(std::uint32_t address);
    [[nodiscard]] EclRunResult run(std::size_t budget = 1000);
    // Text expects no choice; menus use zero-based indices (PARLAY stores its value).
    // Invalid/stale replies do not change machine state or spend the pending request.
    bool resume(std::uint64_t request_id, std::optional<std::size_t> choice = std::nullopt);
    bool resume_input(std::uint64_t request_id, std::string_view input);
    bool resume_host(std::uint64_t request_id, const EclHostReply &reply);
    // Host dice share the checkpointed script RNG. Returns [0, count).
    unsigned host_random(std::uint64_t request_id, unsigned count);

    [[nodiscard]] EclState state() const noexcept
    {
        return state_;
    }

    [[nodiscard]] std::uint32_t address() const noexcept
    {
        return pc_;
    }

    [[nodiscard]] const auto &trace() const noexcept
    {
        return trace_;
    }

  private:
    friend struct opengold::SaveCodec;
    std::shared_ptr<const EclProgram> program_;
    EclState state_{EclState::idle};
    std::uint32_t pc_{};
    std::map<std::uint16_t, std::uint16_t> variables_;
    std::vector<std::uint8_t> image_;
    std::mt19937 random_{5489U}; // Reproducible OpenGoldBox RNG, not the DOS RNG sequence.
    std::set<std::uint8_t> host_opcodes_;
    std::map<std::uint32_t, std::uint32_t> instruction_spans_;
    std::vector<std::uint32_t> stack_, trace_;
    EclConditions conditions_{};
    std::optional<EclRequest> pending_;
    std::optional<std::uint16_t> destination_;
    std::vector<std::uint16_t> menu_values_;
    std::uint64_t next_request_{1}, total_instructions_{};
    std::string diagnostic_;
    [[nodiscard]] EclInstruction decode(std::uint32_t address);
    [[nodiscard]] std::uint16_t value(const EclOperand &arg) const;
    [[nodiscard]] std::uint16_t destination(const EclOperand &arg) const;
    [[nodiscard]] std::string text(const EclOperand &arg) const;
    void require_configurable() const;
    void write(std::uint16_t address, std::uint16_t value);
    void write_string(std::uint16_t address, std::string_view value);
    void finish_request();
    void request_host(const EclInstruction &instruction);
    void jump(std::uint32_t address);
    void execute(const EclInstruction &instruction);
};
} // namespace opengold::por
#endif
