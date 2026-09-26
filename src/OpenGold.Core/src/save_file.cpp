#include "opengold/save_file.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace opengold
{
namespace
{
void require(bool ok, const char *message)
{
    if (!ok)
        throw std::runtime_error(message);
}

class TemporaryFile
{
  public:
    explicit TemporaryFile(std::filesystem::path path) : path_(std::move(path))
    {
    }
    ~TemporaryFile()
    {
        if (owned_)
        {
            std::error_code ignored;
            std::filesystem::remove(path_, ignored);
        }
    }
    TemporaryFile(const TemporaryFile &) = delete;
    TemporaryFile &operator=(const TemporaryFile &) = delete;
    const std::filesystem::path &path() const noexcept
    {
        return path_;
    }
    void acquired() noexcept
    {
        owned_ = true;
    }
    void installed() noexcept
    {
        owned_ = false;
    }

  private:
    std::filesystem::path path_;
    bool owned_{};
};

#ifdef _WIN32
struct CloseHandleOwner
{
    void operator()(void *handle) const noexcept
    {
        CloseHandle(handle);
    }
};
#else
class Descriptor
{
  public:
    explicit Descriptor(int fd) noexcept : fd_(fd)
    {
    }
    ~Descriptor()
    {
        if (fd_ >= 0)
            ::close(fd_);
    }
    Descriptor(const Descriptor &) = delete;
    Descriptor &operator=(const Descriptor &) = delete;
    int get() const noexcept
    {
        return fd_;
    }

  private:
    int fd_;
};
#endif

void write_temporary(TemporaryFile &file, std::string_view bytes)
{
#ifdef _WIN32
    const auto raw = CreateFileW(file.path().c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                 FILE_ATTRIBUTE_NORMAL, nullptr);
    require(raw != INVALID_HANDLE_VALUE, "Cannot create temporary save");
    std::unique_ptr<void, CloseHandleOwner> handle(raw);
    file.acquired();
    std::size_t offset = 0;
    while (offset < bytes.size())
    {
        const auto count =
            static_cast<DWORD>(std::min<std::size_t>(bytes.size() - offset, MAXDWORD));
        DWORD written{};
        require(WriteFile(handle.get(), bytes.data() + offset, count, &written, nullptr) && written,
                "Cannot write save");
        offset += written;
    }
    require(FlushFileBuffers(handle.get()), "Cannot flush save");
#else
    Descriptor handle(::open(file.path().c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600));
    require(handle.get() >= 0, "Cannot create temporary save");
    file.acquired();
    std::size_t offset = 0;
    while (offset < bytes.size())
    {
        const auto count = ::write(handle.get(), bytes.data() + offset, bytes.size() - offset);
        if (count < 0 && errno == EINTR)
            continue;
        require(count > 0, "Cannot write save");
        offset += static_cast<std::size_t>(count);
    }
    require(::fsync(handle.get()) == 0, "Cannot flush save");
#endif
}
} // namespace

std::string read_save_file(const std::filesystem::path &path, std::size_t limit)
{
    const auto size = std::filesystem::file_size(path);
    require(size <= limit &&
                size <= static_cast<std::uintmax_t>(std::numeric_limits<std::streamsize>::max()),
            "Save file too large");
    std::ifstream input(path, std::ios::binary);
    require(bool(input), "Cannot open save file");
    std::string bytes(static_cast<std::size_t>(size), '\0');
    input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    require(input && input.peek() == std::char_traits<char>::eof() && !input.bad(),
            "Incomplete save file read");
    return bytes;
}

void write_save_file(const std::filesystem::path &path, std::string_view bytes, std::size_t limit)
{
    require(bytes.size() <= limit, "Save file too large");
    const auto directory = path.has_parent_path() ? path.parent_path() : std::filesystem::path(".");
    std::filesystem::create_directories(directory);
    static std::atomic<std::uint64_t> sequence{
        static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count())};
    auto temporary = path;
    temporary += ".tmp." + std::to_string(sequence.fetch_add(1, std::memory_order_relaxed));
    auto backup = path;
    backup += ".bak";
    TemporaryFile file(std::move(temporary));
    write_temporary(file, bytes);
    require(read_save_file(file.path(), limit) == bytes, "Save verification failed");
#ifdef _WIN32
    if (std::filesystem::exists(path))
        require(
            ReplaceFileW(path.c_str(), file.path().c_str(), backup.c_str(), 0, nullptr, nullptr),
            "Cannot replace save; previous file retained");
    else
        require(MoveFileExW(file.path().c_str(), path.c_str(), MOVEFILE_WRITE_THROUGH),
                "Cannot install save");
    file.installed();
#else
    if (std::filesystem::exists(path))
        std::filesystem::copy_file(path, backup, std::filesystem::copy_options::overwrite_existing);
    std::filesystem::rename(file.path(), path);
    file.installed();
    Descriptor parent(::open(directory.c_str(), O_RDONLY | O_DIRECTORY));
    require(parent.get() >= 0 && ::fsync(parent.get()) == 0, "Cannot flush save directory");
#endif
}
} // namespace opengold
