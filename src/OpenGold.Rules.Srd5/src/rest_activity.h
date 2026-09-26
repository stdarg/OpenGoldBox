#pragma once
#include "opengold/rules.h"
namespace opengold::srd5::rest
{
rules::RestPolicy policy(rules::RestKind);
rules::RestProgress begin(rules::RestKind);
rules::RestTransition advance(const rules::RestProgress &, std::uint64_t, rules::RestWork);
rules::RestTransition interrupt(const rules::RestProgress &, rules::RestInterruption);
rules::RestProgress resume(const rules::RestProgress &);
std::uint64_t remaining(const rules::RestProgress &);
void validate(const rules::RestProgress &);
} // namespace opengold::srd5::rest
