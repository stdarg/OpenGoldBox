#ifndef OPENGOLD_TEST_SUPPORT_H
#define OPENGOLD_TEST_SUPPORT_H
#include <exception>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace opengold::test
{
// A failed check. rejects() and throws() let it through, so a broken
// expectation inside an operation is never mistaken for that operation being
// rejected.
struct CheckFailure : std::runtime_error
{
    using std::runtime_error::runtime_error;
};

inline void check(bool ok, const std::string &message)
{
    if (!ok)
    {
        throw CheckFailure(message);
    }
}

// Whether `operation` throws `Expected` (any standard exception by default).
template <class Expected = std::exception, class Operation>
[[nodiscard]] bool throws(Operation &&operation)
{
    try
    {
        std::forward<Operation>(operation)();
    }
    catch (const CheckFailure &)
    {
        throw;
    }
    catch (const Expected &)
    {
        return true;
    }
    return false;
}

// Checks that `operation` throws `Expected` (any standard exception by default).
template <class Expected = std::exception, class Operation>
void rejects(Operation &&operation, const std::string &message = "The operation must be rejected")
{
    check(throws<Expected>(std::forward<Operation>(operation)), message);
}

// Checks that `operation` throws `Expected` and that its message contains
// `diagnostic`, so a rejection for some other reason does not pass.
template <class Expected = std::exception, class Operation>
void rejects_mentioning(Operation &&operation, std::string_view diagnostic)
{
    try
    {
        std::forward<Operation>(operation)();
    }
    catch (const CheckFailure &)
    {
        throw;
    }
    catch (const Expected &error)
    {
        check(std::string_view(error.what()).find(diagnostic) != std::string_view::npos,
              "Unexpected rejection diagnostic: " + std::string(error.what()));
        return;
    }
    check(false, "The operation must be rejected");
}
} // namespace opengold::test
#endif
