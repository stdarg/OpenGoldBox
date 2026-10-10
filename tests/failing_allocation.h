#ifndef OPENGOLD_TESTS_FAILING_ALLOCATION_H
#define OPENGOLD_TESTS_FAILING_ALLOCATION_H
// Fault injection for tests of the strong guarantee (Effective C++ Item 29).
// Valid content never trips the code's consistency checks, so these tests
// make a chosen memory allocation fail instead. A test executable using this
// links failing_allocation.cpp, which replaces the global operator new.
#include <atomic>

namespace opengold::test
{
// Whether only the chosen allocation fails, or it and every one after it.
enum class FailureSpan
{
    one,
    rest
};

// Allocations left before one fails; negative means none fails.
inline std::atomic<long> allocations_until_failure{-1};
inline std::atomic<bool> keep_failing{false};
inline std::atomic<bool> failure_happened{false};

// Arms a failure for its lifetime and disarms it however the scope ends.
class FailingAllocation
{
  public:
    FailingAllocation(long allocations_before_failure, FailureSpan span)
    {
        failure_happened = false;
        keep_failing = span == FailureSpan::rest;
        allocations_until_failure = allocations_before_failure;
    }

    ~FailingAllocation()
    {
        allocations_until_failure = -1;
    }

    FailingAllocation(const FailingAllocation &) = delete;
    FailingAllocation &operator=(const FailingAllocation &) = delete;

    // Whether the code under test reached the armed allocation, which failed.
    [[nodiscard]] bool failed() const
    {
        return failure_happened;
    }
};
} // namespace opengold::test

#endif
