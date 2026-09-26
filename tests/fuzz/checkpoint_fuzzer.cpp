#include "fuzz_cases.h"
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    opengold::test::exercise_checkpoint({data, size});
    return 0;
}
