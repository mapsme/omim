#pragma once

#include <future>
#include <memory>
#include <utility>

namespace base
{
namespace threads
{
// AsyncFinishFlag is used for completion wait of all async tasks.
// AsyncFinishFlag::GelLock() method makes AsyncFinishFlag::Lock object with shared lock of completion wait.
// AsyncFinishFlag::Lock object must be bound to each async awaiting task.
// AsyncFinishFlag::Lock object releases shared lock automatically (in destructor) or by method
// AsyncFinishFlag::Lock::release().
class AsyncFinishFlag
{
public:
  using Lock = std::shared_ptr<void>;

  AsyncFinishFlag()
  {
    auto finishSignalSender = std::make_shared<std::promise<void>>();
    m_finishSlot = finishSignalSender->get_future();
    m_finishLock = Lock{nullptr /* unused value */, [
      signalSender = std::move(finishSignalSender) // lock object does not depend on this object
    ](...) {
      signalSender->set_value();
    }};
  }

  ~AsyncFinishFlag() = default;

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
