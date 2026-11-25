#include "history_entries.h"

#include <algorithm>
#include <charconv>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace opengold::original::history
{
std::string encode_training(const Training &training)
{
    return std::to_string(training_data_version) + ' ' + std::to_string(training.classes) + ' ' +
           std::to_string(training.rolled);
}

Training decode_training(std::string_view data)
{
    std::istringstream in{std::string(data)};
    int version{};
    Training training;
    std::string rest;
    in >> version >> training.classes >> training.rolled;
    if (!in || version != training_data_version || (in >> rest) || training.classes == 0 ||
        training.classes >= (1U << class_slot_count) || training.rolled < 1)
    {
        throw std::invalid_argument("Unreadable original-rules training");
    }
    return training;
}

std::string encode_drain(const Drain &drain)
{
    std::string data(drain_word);
    for (const int level : drain.levels)
    {
        data += ' ' + std::to_string(level);
    }
    return data + ' ' + std::to_string(drain.hit_points);
}

std::string encode_native_drain(const NativeDrain &loss)
{
    auto data = std::string(loss.before_level > 0 ? exact_native_drain_word : native_drain_word) +
                std::to_string(loss.slot) + ' ' + std::to_string(loss.hit_points) + ' ' +
                std::to_string(loss.rolled_hit_points) + ' ' + std::to_string(loss.experience) +
                ' ' + std::to_string(loss.dead);
    if (loss.before_level > 0)
    {
        data += ' ' + std::to_string(loss.before_level) + ' ' +
                std::to_string(loss.previous_experience) + ' ' +
                std::to_string(loss.previous_max_hit_points);
    }
    return data;
}

std::optional<NativeDrain> decode_native_drain(std::string_view data)
{
    const bool exact = data.starts_with(exact_native_drain_word);
    if (!exact && !data.starts_with(native_drain_word))
    {
        return std::nullopt;
    }
    std::istringstream input{std::string(
        data.substr(exact ? exact_native_drain_word.size() : native_drain_word.size()))};
    NativeDrain loss;
    std::string rest;
    if (!(input >> loss.slot >> loss.hit_points >> loss.rolled_hit_points >> loss.experience >>
          loss.dead) ||
        (exact && !(input >> loss.before_level >> loss.previous_experience >>
                    loss.previous_max_hit_points)) ||
        (input >> rest) || loss.slot >= class_slot_count ||
        loss.slot == static_cast<unsigned>(ClassSlot::druid) || loss.hit_points < 0 ||
        loss.rolled_hit_points < 0 ||
        loss.experience > static_cast<unsigned>(std::numeric_limits<int>::max()) ||
        loss.previous_experience > static_cast<unsigned>(std::numeric_limits<int>::max()) ||
        (exact && (loss.before_level < 1 || loss.previous_max_hit_points < 0)))
    {
        throw std::invalid_argument("Unreadable native scripted drain");
    }
    return loss;
}

std::string encode_raised(rules::Revival by)
{
    return std::string(raised_word) + (by == rules::Revival::temple ? " temple" : " spell");
}

std::optional<rules::Revival> decode_raised(std::string_view data)
{
    if (data == encode_raised(rules::Revival::temple))
    {
        return rules::Revival::temple;
    }
    if (data == encode_raised(rules::Revival::spell))
    {
        return rules::Revival::spell;
    }
    if (data.starts_with(raised_word))
    {
        throw std::invalid_argument("Unreadable original-rules raising");
    }
    return std::nullopt;
}

std::optional<std::uint64_t> decode_krynn_calendar(std::string_view data)
{
    if (!data.starts_with(krynn_calendar_word))
    {
        return std::nullopt;
    }
    data.remove_prefix(krynn_calendar_word.size());
    std::uint64_t days{};
    const auto [end, error] = std::from_chars(data.data(), data.data() + data.size(), days);
    if (error != std::errc{} || end != data.data() + data.size() ||
        days > std::numeric_limits<std::uint64_t>::max() / 1440 + 1)
    {
        throw std::invalid_argument("Unreadable Krynn calendar");
    }
    return days;
}

std::string encode_age(int years)
{
    return std::string(age_word) + ' ' + std::to_string(years);
}

std::optional<int> decode_age(std::string_view data)
{
    if (!data.starts_with(age_word))
    {
        return std::nullopt;
    }
    std::istringstream in{std::string(data.substr(age_word.size()))};
    int years{};
    std::string rest;
    if (!(in >> years) || (in >> rest) || years == 0)
    {
        throw std::invalid_argument("Unreadable original-rules age");
    }
    return years;
}

std::string encode_charisma_loss(int points)
{
    return std::string(charisma_word) + ' ' + std::to_string(points);
}

std::optional<int> decode_charisma_loss(std::string_view data)
{
    if (!data.starts_with(charisma_word))
    {
        return std::nullopt;
    }
    std::istringstream in{std::string(data.substr(charisma_word.size()))};
    int points{};
    std::string rest;
    if (!(in >> points) || (in >> rest) || points <= 0)
    {
        throw std::invalid_argument("Unreadable original-rules Charisma loss");
    }
    return points;
}

std::optional<Drain> decode_drain(std::string_view data)
{
    if (!data.starts_with(drain_word))
    {
        return std::nullopt;
    }
    std::istringstream in{std::string(data.substr(drain_word.size()))};
    Drain drain;
    for (int &level : drain.levels)
    {
        in >> level;
    }
    in >> drain.hit_points;
    std::string rest;
    if (!in || (in >> rest) || drain.hit_points < 0 ||
        std::ranges::any_of(
            drain.levels,
            [](int level)
            {
                return level < 0;
            }))
    {
        throw std::invalid_argument("Unreadable original-rules level drain");
    }
    return drain;
}

std::string encode_restoration(const Restoration &restoration)
{
    auto data = std::string(restoration.native_experience ? native_restore_word : restore_word) +
                ' ' + std::to_string(static_cast<unsigned>(restoration.slot)) + ' ' +
                std::to_string(restoration.hit_points) + ' ' +
                std::to_string(restoration.rolled_hit_points);
    if (restoration.native_experience)
    {
        data += ' ' + std::to_string(*restoration.native_experience);
    }
    return data;
}

std::optional<Restoration> decode_restoration(std::string_view data)
{
    const bool native = data.starts_with(native_restore_word);
    if (!native && !data.starts_with(restore_word))
    {
        return std::nullopt;
    }
    std::istringstream in{
        std::string(data.substr(native ? native_restore_word.size() : restore_word.size()))};
    unsigned slot{}, experience{};
    Restoration restoration;
    std::string rest;
    if (!(in >> slot >> restoration.hit_points >> restoration.rolled_hit_points) ||
        (native && !(in >> experience)) || (in >> rest) || slot >= class_slot_count ||
        restoration.hit_points < 0 || restoration.rolled_hit_points < 0 ||
        experience > static_cast<unsigned>(std::numeric_limits<int>::max()))
    {
        throw std::invalid_argument("Unreadable original-rules restoration");
    }
    restoration.slot = static_cast<ClassSlot>(slot);
    if (native)
    {
        restoration.native_experience = experience;
    }
    return restoration;
}
} // namespace opengold::original::history
