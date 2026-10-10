#ifndef OPENGOLDBOX_GUARDED_HANDLERS_H
#define OPENGOLDBOX_GUARDED_HANDLERS_H

#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <cstdint>
#include <cstring>
#include <exception>
#include <type_traits>
#include <utility>

namespace presentation
{
// Every guarded view says how it shows a failed handler, even if only by
// leaving it to the log. Stated as a requirement, a misspelled or private
// report_failure is a compile error rather than silently skipped (Effective
// C++ Item 41).
template <class View>
concept ReportsFailures = requires(View &view, const std::exception &failure)
{
    view.report_failure(failure);
};

// Logs a failed handler and passes it to the view. Instantiated once per
// view rather than once per handler, since it depends on nothing else
// (Effective C++ Item 44).
template <ReportsFailures View>
void report_guarded_failure(View &view, const std::exception &failure) noexcept
{
    godot::UtilityFunctions::push_error(godot::String::utf8(failure.what()));
    try
    {
        view.report_failure(failure);
    }
    catch (const std::exception &)
    {
        // Already logged above; the view could not show it.
    }
}

// Godot calls handlers from engine code that a C++ exception cannot unwind
// through, so an escaping exception ends the game. run_guarded reports a
// failure through report_guarded_failure instead.
template <ReportsFailures View, class Work> void run_guarded(View &view, Work &&work) noexcept
{
    try
    {
        std::forward<Work>(work)();
    }
    catch (const std::exception &failure)
    {
        report_guarded_failure(view, failure);
    }
}

// callable_mp's method pointer with run_guarded around the call. A handler
// that fails returns a default value.
template <ReportsFailures T, class R, class... P>
class GuardedMethodPointer final : public godot::CallableCustomMethodPointerBase
{
  public:
    GuardedMethodPointer(T *instance, R (T::*method)(P...))
    {
        std::memset(&data_, 0, sizeof(Data));
        data_.instance = instance;
        data_.method = method;
        // The base hashes and compares the bytes of data_, as callable_mp does.
        _setup(reinterpret_cast<std::uint32_t *>(&data_), sizeof(Data));
    }

    godot::ObjectID get_object() const override
    {
        return godot::ObjectID(data_.instance->get_instance_id());
    }

    int get_argument_count(bool &is_valid) const override
    {
        is_valid = true;
        return sizeof...(P);
    }

    void call(const godot::Variant **arguments, int count, godot::Variant &result,
              GDExtensionCallError &error) const override
    {
        run_guarded(*data_.instance, [&]
        {
            invoke(arguments, count, result, error);
        });
    }

  private:
    void invoke(const godot::Variant **arguments, int count, godot::Variant &result,
                GDExtensionCallError &error) const
    {
        if constexpr (std::is_void_v<R>)
        {
            godot::call_with_variant_args(data_.instance, data_.method, arguments, count, error);
        }
        else
        {
            godot::call_with_variant_args_ret(data_.instance, data_.method, arguments, count,
                                              result, error);
        }
    }

    struct Data
    {
        T *instance;
        R (T::*method)(P...);
    } data_;
    static_assert(sizeof(Data) % 4 == 0);
};

// Use in place of callable_mp(instance, method) for a signal handler.
template <ReportsFailures T, class R, class... P>
[[nodiscard]] godot::Callable guarded(T *instance, R (T::*method)(P...))
{
    using Pointer = GuardedMethodPointer<T, R, P...>;
    return godot::internal::create_callable_from_ccmp(memnew(Pointer(instance, method)));
}
} // namespace presentation

#endif
