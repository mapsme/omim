#pragma once

#include "base/thread.hpp"

#include <condition_variable>
#include <mutex>
#include <queue>
#include <utility>

namespace base
{
class WorkerThread
{
public:
  enum class Exit
  {
    ExecPending,
    SkipPending
  };

  using Task = std::function<void()>;

  WorkerThread();
  ~WorkerThread();

  template <typename T>
  void Push(T && t)
  {
    std::lock_guard<std::mutex> lk(m_mu);
    m_queue.emplace(std::forward<T>(t));
    m_cv.notify_one();
  }

  void Shutdown(Exit e);

private:
  void Worker();

  threads::SimpleThread m_thread;
  std::mutex m_mu;
  std::condition_variable m_cv;

  bool m_shutdown = false;
  Exit m_exit = Exit::SkipPending;

  std::queue<Task> m_queue;
};
}  // namespace base
