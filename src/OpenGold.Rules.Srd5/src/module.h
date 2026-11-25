#ifndef OPENGOLD_SRD5_MODULE_H
#define OPENGOLD_SRD5_MODULE_H
#include "opengold/rules.h"
#include <memory>
#include <string_view>

namespace opengold::srd5::detail
{
// The SRD campaign rules module `content_bytes` describe; it creates and
// restores the fights (module.cpp).
std::unique_ptr<rules::RulesModule> make_module(std::string_view content_bytes);
} // namespace opengold::srd5::detail
#endif
