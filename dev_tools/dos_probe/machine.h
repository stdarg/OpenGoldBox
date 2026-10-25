#ifndef OPENGOLD_DOS_PROBE_MACHINE_H
#define OPENGOLD_DOS_PROBE_MACHINE_H

#include "cpu.h"
#include "ega.h"

#include <cstdint>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace opengold::probe
{
// A PC with an EGA card and just enough BIOS and DOS to run the original game
// headless, deterministically: time is counted in instructions, so a run with
// the same keys always takes the same path. Drive C:\POOLRAD reads the game
// directory and never writes it: every file the program creates or opens for
// writing lives in the scratch directory, which reads see first.
class Machine final : public Bus
{
  public:
    struct Options
    {
        std::filesystem::path game_directory;
        std::filesystem::path scratch_directory;
        // One 18.2 Hz timer tick, at the PIT's default rate.
        std::uint64_t instructions_per_tick{20000};
        // Hides the installation's SAVE folder, so a run starts with no saved
        // games or characters whatever the player has.
        bool fresh_saves{};
    };

    // Called on every access to a watched linear address range.
    using Watch = std::function<void(std::uint32_t linear, std::uint8_t value, bool write)>;

    explicit Machine(Options options);

    // Loads an EXE from drive C (for example C:\POOLRAD\START.EXE) as DOS would.
    void load(const std::string &dos_path);
    // Runs until `instructions` more have run, the program exits, or `stop`
    // (checked after each instruction) holds.
    void run(std::uint64_t instructions, const std::function<bool()> &stop = {});

    // Queues a key: its scan code reaches port 60h and INT 9, and the BIOS
    // buffer then holds it for INT 16h.
    void press(std::uint8_t scan_code, std::uint8_t ascii);

    // Whether the program has asked for a key and found none for a while since
    // the last key was read: it is waiting for the player. Work that checks
    // the keyboard between steps, as setting up a fight does, polls too, so
    // the polls must go on for `quiet_instructions`.
    [[nodiscard]] bool waiting_for_key() const
    {
        return keys_.empty() && !irq_pending_[1] && !pending_scan_ && ram_[0x41A] == ram_[0x41C] &&
               idle_polls_ >= 3 && instructions_ - idle_since_ >= quiet_instructions;
    }

    [[nodiscard]] bool exited() const
    {
        return exit_code_.has_value();
    }

    [[nodiscard]] std::optional<int> exit_code() const
    {
        return exit_code_;
    }

    [[nodiscard]] std::uint64_t instructions() const
    {
        return instructions_;
    }

    // Text the program wrote through DOS or the BIOS teletype.
    [[nodiscard]] const std::string &console() const
    {
        return console_;
    }

    [[nodiscard]] std::vector<std::uint8_t> screen() const
    {
        return ega_.rgb_screen();
    }

    Cpu &cpu()
    {
        return cpu_;
    }

    [[nodiscard]] std::uint8_t peek(std::uint32_t linear) const
    {
        return linear < ram_.size() ? ram_[linear] : 0;
    }

    void poke(std::uint32_t linear, std::uint8_t value)
    {
        if (linear < ram_.size())
        {
            ram_[linear] = value;
        }
    }

    // Reads every file the program opens, in order, for a trace.
    [[nodiscard]] const std::vector<std::string> &opened_files() const
    {
        return opened_files_;
    }

    // Called with the CPU before every instruction while set, such as to see
    // which code runs; empty to stop.
    void before_instruction(std::function<void(const Cpu &)> callback)
    {
        before_instruction_ = std::move(callback);
    }

    void watch(std::uint32_t first, std::uint32_t last, Watch callback);

    void clear_watches()
    {
        watches_.clear();
    }

    // How often each BIOS or DOS service (interrupt and AH) was called since
    // the counts were last taken.
    [[nodiscard]] std::map<std::pair<std::uint8_t, std::uint8_t>, unsigned> take_service_counts()
    {
        return std::exchange(service_counts_, {});
    }

    // Bus
    std::uint8_t read(std::uint32_t linear) override;
    void write(std::uint32_t linear, std::uint8_t value) override;
    std::uint8_t in(std::uint16_t port) override;
    void out(std::uint16_t port, std::uint8_t value) override;
    void host_interrupt(std::uint8_t number) override;

  private:
    struct OpenFile
    {
        std::fstream stream;
        std::string dos_path;
    };

    struct WatchRange
    {
        std::uint32_t first, last;
        Watch callback;
    };

    struct Search
    {
        std::vector<std::string> names;
        std::size_t next{};
    };

    void install_bios();
    void video_service();
    void keyboard_service();
    void timer_tick();
    void keyboard_interrupt();
    void dos_service();
    void file_service(std::uint8_t function);
    void memory_service(std::uint8_t function);
    void find_service(std::uint8_t function);
    void return_carry(bool failed, std::uint16_t error = 0);
    void set_stacked_flag(Flag flag, bool on);

    // The program looked for a key and found none.
    void note_idle_poll()
    {
        if (idle_polls_++ == 0)
        {
            idle_since_ = instructions_;
        }
    }

    std::string read_string(std::uint16_t segment, std::uint16_t offset) const;
    // The DOS path's directory and name, upper case, relative to C:\POOLRAD;
    // empty when it lies outside it.
    std::optional<std::string> game_relative(const std::string &dos_path) const;
    std::filesystem::path read_path(const std::string &relative) const;
    // The installation's copy of a file, unless the run hides it.
    std::optional<std::filesystem::path> original_path(const std::string &relative) const;
    std::filesystem::path write_path(const std::string &relative) const;
    int open_file(const std::string &dos_path, std::uint8_t mode, bool create);

    Options options_;
    std::vector<std::uint8_t> ram_;
    Ega ega_;
    Cpu cpu_{*this};
    std::uint64_t instructions_{};
    std::uint64_t next_tick_{};
    std::optional<int> exit_code_;
    std::string console_;
    std::vector<WatchRange> watches_;
    std::map<std::pair<std::uint8_t, std::uint8_t>, unsigned> service_counts_;
    std::function<void(const Cpu &)> before_instruction_;

    // Keyboard: keys still to deliver, and the scan code port 60h shows.
    std::deque<std::pair<std::uint8_t, std::uint8_t>> keys_;
    std::uint8_t port60_{};
    std::uint8_t pending_ascii_{};
    // The scan code DOS still owes for an extended key it returned as 0.
    std::optional<std::uint8_t> pending_scan_;
    std::uint64_t key_ready_at_{};
    unsigned idle_polls_{};
    std::uint64_t idle_since_{};
    static constexpr std::uint64_t quiet_instructions = 3'000'000;
    bool irq_pending_[2]{};

    std::uint8_t pic_mask_{};
    std::uint16_t pit_divisor_{0};
    unsigned pit_byte_{};
    std::uint8_t port61_{};
    std::uint8_t video_mode_{3};

    // DOS
    std::map<int, std::shared_ptr<OpenFile>> files_;
    std::set<std::string> deleted_;
    std::vector<std::string> opened_files_;
    std::map<std::uint32_t, Search> searches_;
    std::uint16_t psp_{}, dta_segment_{}, dta_offset_{0x80};
    std::string current_directory_{"POOLRAD"};
    // Allocated blocks: segment and size in paragraphs, sorted.
    std::map<std::uint16_t, std::uint16_t> blocks_;
};
} // namespace opengold::probe

#endif
