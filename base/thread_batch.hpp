#pragma once

#include "base/thread_pool_computational.hpp"

#include <experimental/tuple>

#include <mutex>
#include <vector>
#include <tuple>
#include <utility>

namespace base
{
namespace threads
{
class BatchSubmitter
{
public:
  BatchSubmitter(thread_pool::computational::ThreadPool & threadPool, size_t batchSize)
    : m_threadPool{threadPool}, m_batchSize{batchSize}
  {
    CHECK_GREATER(batchSize, 0, ());
  }

  ~BatchSubmitter()
  {
    Flush();
  }

  template <typename F, typename... Args>
  void operator()(F && fn, Args &&... args)
  {
    {
      std::unique_lock<std::mutex> lock(m_mutex);
      m_tasks.emplace_back([
        fn = std::forward<F>(fn),
        args = std::make_tuple(std::forward<Args>(args)...)
      ] () mutable {
        std::experimental::apply(std::forward<F>(fn), std::move(args));
      });
    }

    if (m_batchSize <= m_tasks.size())
      Flush();
  }

private:
  void Flush()
  {
    if (m_tasks.empty())
      return;

    m_threadPool.Submit([tasks = std::move(m_tasks)]() mutable {
      for (auto & task : tasks)
        task();
    });
  }

  std::mutex m_mutex;
  thread_pool::computational::ThreadPool & m_threadPool;
  size_t m_batchSize;
  std::vector<std::function<void()>> m_tasks;
};
}  // namespace threads
}  // namespace base
