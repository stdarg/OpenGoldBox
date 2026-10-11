#ifndef OPENGOLD_SRD5_CONCENTRATION_H
#define OPENGOLD_SRD5_CONCENTRATION_H
#include "status_effects.h"
#include <algorithm>
#include <charconv>
#include <istream>
#include <ostream>
#include <stdexcept>
#include <utility>

namespace opengold::srd5::detail
{
// Identifies an application, not just a spell: a recast must retire the old area.
struct ConcentrationSource
{
    std::uint64_t scope{}, application{};
    rules::EntityId caster{};
    bool operator==(const ConcentrationSource &) const = default;
};

struct Concentration
{
    ConcentrationSource source;
    std::uint64_t remaining_ms{};
    bool operator==(const Concentration &) const = default;
};

// One instance per owner. Returned sources tell the integration which effects
// to remove; this state machine never owns or reaches into combat actors.
class ConcentrationState
{
  public:
    const std::optional<Concentration> &active() const
    {
        return active_;
    }

    // Named start and stop, not begin and end, which read as iterators at a
    // call site (Effective C++ Item 18).
    std::optional<ConcentrationSource> stop()
    {
        auto old = std::exchange(active_, std::nullopt);
        return old ? std::optional(old->source) : std::nullopt;
    }

    std::optional<ConcentrationSource> start(Concentration next, bool incapacitated_or_dead = false)
    {
        if (!next.source.scope || !next.source.application || !next.source.caster ||
                !next.remaining_ms || incapacitated_or_dead)
            throw std::runtime_error("Invalid concentration start");
        auto old = stop();
        active_ = next;
        return old;
    }

    std::optional<ConcentrationSource> elapse(std::uint64_t milliseconds)
    {
        if (!active_)
            return std::nullopt;
        if (milliseconds >= active_->remaining_ms)
            return stop();
        active_->remaining_ms -= milliseconds;
        return std::nullopt;
    }

    struct DamageResult
    {
        std::optional<SaveResult> save;
        std::optional<ConcentrationSource> ended;
    };

    // damage_taken is post-defense damage, BEFORE subtracting Temporary HP.
    // Check incapacitation/death after applying damage; these end without a roll.
    DamageResult damage(int damage_taken, int constitution_save, RollModifiers modifiers,
                        bool incapacitated_or_dead, std::uint64_t &rng)
    {
        if (damage_taken < 0)
            throw std::runtime_error("Invalid concentration damage");
        if (!active_)
            return {};
        if (incapacitated_or_dead)
            return {std::nullopt, stop()};
        if (!damage_taken)
            return {};
        auto save = saving_throw(Ability::constitution, constitution_save,
                                 std::clamp(damage_taken / 2, 10, 30), modifiers, rng);
        return {save, save.success ? std::nullopt : stop()};
    }

    std::optional<ConcentrationSource> incapacitate()
    {
        return stop();
    }

    bool operator==(const ConcentrationState &) const = default;

  private:
    std::optional<Concentration> active_;
};

inline void write_concentration(std::ostream &out, const ConcentrationState &state)
{
    out << "CN1 " << static_cast<int>(state.active().has_value());
    if (const auto &value = state.active())
        out << ' ' << value->source.scope << ' ' << value->source.application << ' '
            << value->source.caster << ' ' << value->remaining_ms;
}

// Parse into a fresh value; malformed fields cannot partially replace an owner.
// The enclosing combat/campaign reader must additionally validate the source
// against its owner and effect registry and enforce the named spell's duration.
inline ConcentrationState read_concentration(std::istream &in)
{
    const auto number = [&](auto & value)
    {
        std::string token;
        in >> token;
        const auto parsed = std::from_chars(token.data(), token.data() + token.size(), value);
        if (!in || parsed.ec != std::errc{} || parsed.ptr != token.data() + token.size())
            throw std::runtime_error("Invalid concentration field");
    };
    std::string magic;
    in >> magic;
    unsigned present{};
    number(present);
    if (magic != "CN1" || present > 1)
        throw std::runtime_error("Invalid concentration record");
    ConcentrationState result;
    if (present)
    {
        Concentration value;
        number(value.source.scope);
        number(value.source.application);
        number(value.source.caster);
        number(value.remaining_ms);
        result.start(value);
    }
    return result;
}
} // namespace opengold::srd5::detail
#endif
