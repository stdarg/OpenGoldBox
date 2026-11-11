#ifndef OPENGOLDBOX_UNLOADABLE_ART_H
#define OPENGOLDBOX_UNLOADABLE_ART_H

#include "opengold/diagnostic_log.h"
#include <span>
#include <string>
#include <string_view>

// New art the game cannot load shows as the original art it replaces; the log
// names the file, so whoever made the art can mend it.
inline void log_unloadable_art(std::string_view problem)
{
    opengold::log_diagnostic(opengold::LogPriority::error, "art",
                             std::string(problem) + "; the original art shows instead");
}

inline void log_unloadable_art(std::span<const std::string> problems)
{
    for (const auto &problem : problems)
    {
        log_unloadable_art(std::string_view{problem});
    }
}

#endif
