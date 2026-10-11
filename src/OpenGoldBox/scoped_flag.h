#ifndef OPENGOLDBOX_SCOPED_FLAG_H
#define OPENGOLDBOX_SCOPED_FLAG_H

namespace presentation
{
// Sets a flag for a scope and clears it however the scope ends, so an
// exception cannot leave a view ignoring its controls.
class ScopedFlag
{
  public:
    explicit ScopedFlag(bool &flag) noexcept : flag_(flag)
    {
        flag_ = true;
    }

    ~ScopedFlag()
    {
        flag_ = false;
    }

    ScopedFlag(const ScopedFlag &) = delete;
    ScopedFlag &operator=(const ScopedFlag &) = delete;

  private:
    bool &flag_;
};
} // namespace presentation

#endif
