#ifndef OPENGOLD_SRD5_SCOPE_GUARDS_H
#define OPENGOLD_SRD5_SCOPE_GUARDS_H
#include <utility>

namespace opengold::srd5::detail
{
// Gives `slot` the value `at_exit` when the scope ends, however it ends: a
// flag or an id that holds for the span of one call.
template <typename T> class ResetAtExit
{
  public:
    ResetAtExit(T &slot, T at_exit) : slot_(slot), at_exit_(std::move(at_exit))
    {
    }

    ResetAtExit(const ResetAtExit &) = delete;
    ResetAtExit &operator=(const ResetAtExit &) = delete;

    ~ResetAtExit()
    {
        slot_ = std::move(at_exit_);
    }

  private:
    T &slot_;
    T at_exit_;
};

// Holds `flag` true for the span of the scope.
class SetWhileAlive
{
  public:
    explicit SetWhileAlive(bool &flag) : flag_(flag)
    {
        flag_ = true;
    }

    SetWhileAlive(const SetWhileAlive &) = delete;
    SetWhileAlive &operator=(const SetWhileAlive &) = delete;

    ~SetWhileAlive()
    {
        flag_ = false;
    }

  private:
    bool &flag_;
};
} // namespace opengold::srd5::detail
#endif
