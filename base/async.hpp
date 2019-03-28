#pragma once

#include <future>
#include <memory>
#include <utility>

namespace base
{
namespace threads
{
class AsyncFinishFlag
{
public:
  using Lock = std::shared_ptr<void>;

  AsyncFinishFlag()
  {
    auto fishSignalSender = std::make_shared<std::promise<void>>();
    m_finishSlot = fishSignalSender->get_future();
    m_finishLock = Lock{nullptr, [
      signalSender = std::move(fishSignalSender) // lock object does not depend on this object
    ](...) {
      signalSender->set_value();
    }};
  }

  AsyncFinishFlag(AsyncFinishFlag const &) = delete;
  AsyncFinishFlag & operator=(AsyncFinishFlag const &) = delete;

  Lock GetLock()
  {
    return m_finishLock;
  }

  void Wait()
  {
    m_finishLock.reset();
    m_finishSlot.wait();
  }

private:
  std::future<void> m_finishSlot;
  Lock m_finishLock;
};
}  // namespace threads
}  // namespace base
