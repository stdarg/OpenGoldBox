#ifndef OPENGOLD_ORIGINAL_CHECKPOINT_TEXT_H
#define OPENGOLD_ORIGINAL_CHECKPOINT_TEXT_H
#include <charconv>
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

namespace opengold::original::detail
{
// A checkpoint's text: numbers and length-prefixed strings, space-separated.
class Writer
{
  public:
    template <class... T> Writer &operator()(const T &...values)
    {
        (put(values), ...);
        return *this;
    }

    [[nodiscard]] std::string text() const
    {
        return out_.str();
    }

  private:
    std::ostringstream out_;

    void put(const std::string &text)
    {
        out_ << text.size() << ':' << text << ' ';
    }

    void put(bool value)
    {
        out_ << (value ? 1 : 0) << ' ';
    }

    template <class T>
        requires std::is_integral_v<T>
    void put(T value)
    {
        out_ << +value << ' ';
    }
};

class Reader
{
  public:
    explicit Reader(std::string_view text) : text_(text)
    {
    }

    // Unsigned numbers read as unsigned: the random state uses all 64 bits.
    template <class Number> Number number()
    {
        skip();
        Number value{};
        const auto [end, error] =
            std::from_chars(text_.data() + at_, text_.data() + text_.size(), value);
        if (error != std::errc{})
        {
            throw std::runtime_error("Invalid original-rules combat checkpoint");
        }
        at_ = static_cast<std::size_t>(end - text_.data());
        return value;
    }

    template <class T> T as()
    {
        if constexpr (std::is_same_v<T, std::uint64_t>)
        {
            return number<std::uint64_t>();
        }
        return static_cast<T>(number<long long>());
    }

    std::string text()
    {
        const auto size = static_cast<std::size_t>(number<long long>());
        if (at_ >= text_.size() || text_[at_] != ':' || size > text_.size() - at_ - 1)
        {
            throw std::runtime_error("Invalid original-rules combat checkpoint");
        }
        std::string value(text_.substr(at_ + 1, size));
        at_ += size + 1;
        return value;
    }

    [[nodiscard]] bool finished()
    {
        skip();
        return at_ == text_.size();
    }

  private:
    std::string_view text_;
    std::size_t at_{};

    void skip()
    {
        while (at_ < text_.size() && text_[at_] == ' ')
        {
            ++at_;
        }
    }
};
} // namespace opengold::original::detail
#endif
