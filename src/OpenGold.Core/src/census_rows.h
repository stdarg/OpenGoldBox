#ifndef OPENGOLD_CORE_CENSUS_ROWS_H
#define OPENGOLD_CORE_CENSUS_ROWS_H

#include <exception>
#include <functional>
#include <string>
#include <utility>

#include "opengold/game_census.h"

// Building one census row: the files of a kind and each piece put through its
// decoder (game_census.cpp and ua_census.cpp).
namespace opengold::census_detail
{

class RowBuilder
{
  public:
    explicit RowBuilder(std::string kind)
    {
        row_.kind = std::move(kind);
    }

    void add_file(const std::string &name)
    {
        row_.files.push_back(name);
    }

    // Counts one piece: decoded when the check returns without throwing.
    void check(const std::string &where, const std::function<void()> &decode)
    {
        ++row_.pieces;
        try
        {
            decode();
            ++row_.decoded;
        }
        catch (const std::exception &error)
        {
            fail(where, error.what());
        }
    }

    void fail(const std::string &where, const std::string &why)
    {
        if (row_.first_failure.empty())
        {
            row_.first_failure = where + ": " + why;
        }
    }

    [[nodiscard]] CensusRow take() &&
    {
        return std::move(row_);
    }

  private:
    CensusRow row_;
};

} // namespace opengold::census_detail

#endif
