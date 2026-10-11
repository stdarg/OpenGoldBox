// Replaces the global allocation functions for the tests that link this file,
// so a opengold::test::FailingAllocation can make a chosen allocation throw.
#include "failing_allocation.h"
#include <cstdlib>
#include <new>

void *operator new (std::size_t size)
{
    const long left = opengold::test::allocations_until_failure.load();
    if (left == 0)
    {
        opengold::test::failure_happened = true;
        if (!opengold::test::keep_failing)
            opengold::test::allocations_until_failure = -1;
        throw std::bad_alloc();
    }
    if (left > 0)
        --opengold::test::allocations_until_failure;
    if (void *memory = std::malloc(size == 0 ? 1 : size))
        return memory;
    throw std::bad_alloc();
}

void operator delete (void *memory) noexcept
{
    std::free(memory);
}

void operator delete (void *memory, std::size_t) noexcept
{
    std::free(memory);
}
