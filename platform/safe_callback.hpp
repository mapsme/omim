#pragma once

#include "platform/platform.hpp"

#include <future>
#include <functional>
#include <utility>

namespace platform
{
template <typename T>
class SafeCallback;

// Calls callback on main thread, all params are copied.
// *NOTE* The class is not thread-safe.
template <typename R, typename ...Args>
class SafeCallback<R(Args...)>
{
public:
  SafeCallback() = default;

  template <typename Fn>
  SafeCallback(Fn const & fn)
    : m_fn(fn)
  {
  }

  operator bool() const noexcept
  {
    return static_cast<bool>(m_fn);
  }

  void operator()(Args... args) const
  {
    GetPlatform().RunOnGuiThread(std::bind(m_fn, std::move(args)...));
  }

private:
  std::function<R(Args...)> m_fn;
};
}  // namespace platform
