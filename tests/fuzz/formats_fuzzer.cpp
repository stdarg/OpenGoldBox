#include "fuzz_cases.h"
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    // libFuzzer owns this input; the harness borrows it for this call only.
    opengold::test::exercise_formats({data, size});
    return 0;
}
