#include "base/thread_pool_delayed.hpp"

#include <array>

using namespace std;

namespace base
{
namespace thread_pool
{
namespace delayed
{
ThreadPool::ThreadPool(size_t threadsCount /* = 1 */, Exit e /* = Exit::SkipPending */)
  : m_exit(e)
{
  for (size_t i = 0; i < threadsCount; ++i)
    m_threads.emplace_back(threads::SimpleThread(&ThreadPool::ProcessTasks, this));
}

ThreadPool::~ThreadPool()
{
  ShutdownAndJoin();
}

bool ThreadPool::Push(Task && t)
{
  return TouchQueues([&]() { m_immediate.emplace(move(t)); });
}

bool ThreadPool::Push(Task const & t)
{
  return TouchQueues([&]() { m_immediate.emplace(t); });
}

bool ThreadPool::PushDelayed(Duration const & delay, Task && t)
{
  auto const when = Now() + delay;
  return TouchQueues([&]() { m_delayed.emplace(when, move(t)); });
}

bool ThreadPool::PushDelayed(Duration const & delay, Task const & t)
{
  auto const when = Now() + delay;
  return TouchQueues([&]() { m_delayed.emplace(when, t); });
}

void ThreadPool::ProcessTasks()
{
  ImmediateQueue pendingImmediate;
  DelayedQueue pendingDelayed;

  while (true)
  {
    array<Task, QUEUE_TYPE_COUNT> tasks;

    {
      unique_lock<mutex> lk(m_mu);
      if (!m_delayed.empty())
      {
        // We need to wait until the moment when the earliest delayed
        // task may be executed, given that an immediate task or a
        // delayed task with an earlier execution time may arrive
        // while we are waiting.
        auto const when = m_delayed.top().m_when;
        m_cv.wait_until(lk, when, [this, when]() {
          return m_shutdown || !m_immediate.empty() || m_delayed.empty() ||
              (!m_delayed.empty() && m_delayed.top().m_when < when);
        });
      }
      else
      {
        // When there are no delayed tasks in the queue, we need to
        // wait until there is at least one immediate or delayed task.
        m_cv.wait(lk,
                  [this]() { return m_shutdown || !m_immediate.empty() || !m_delayed.empty(); });
      }

      if (m_shutdown)
      {
        switch (m_exit)
        {
        case Exit::ExecPending:
          ASSERT(pendingImmediate.empty(), ());
          m_immediate.swap(pendingImmediate);

          ASSERT(pendingDelayed.empty(), ());
          m_delayed.swap(pendingDelayed);
          break;
        case Exit::SkipPending: break;
        }

        break;
      }

      auto const canExecImmediate = !m_immediate.empty();
      auto const canExecDelayed = !m_delayed.empty() && Now() >= m_delayed.top().m_when;

      if (canExecImmediate)
      {
        tasks[QUEUE_TYPE_IMMEDIATE] = move(m_immediate.front());
        m_immediate.pop();
      }

      if (canExecDelayed)
      {
        tasks[QUEUE_TYPE_DELAYED] = move(m_delayed.top().m_task);
        m_delayed.pop();
      }
    }

    for (auto const & task : tasks)
    {
      if (task)
        task();
    }
  }

  for (; !pendingImmediate.empty(); pendingImmediate.pop())
    pendingImmediate.front()();

  for (; !pendingDelayed.empty(); pendingDelayed.pop())
  {
    auto const & top = pendingDelayed.top();
    while (true)
    {
      auto const now = Now();
      if (now >= top.m_when)
        break;
      auto const delay = top.m_when - now;
      this_thread::sleep_for(delay);
    }
    ASSERT(Now() >= top.m_when, ());
    top.m_task();
  }
}

bool ThreadPool::Shutdown(Exit e)
{
  lock_guard<mutex> lk(m_mu);
  if (m_shutdown)
    return false;
  m_shutdown = true;
  m_exit = e;
  m_cv.notify_all();
  return true;
}

void ThreadPool::ShutdownAndJoin()
{
  ASSERT(m_checker.CalledOnOriginalThread(), ());
  Shutdown(m_exit);
  for (auto & thread : m_threads)
  {
    if (thread.joinable())
      thread.join();
  }
  m_threads.clear();
}

bool ThreadPool::IsShutDown()
{
  lock_guard<mutex> lk(m_mu);
  return m_shutdown;
}
}  // namespace delayed
}  // namespace thread_pool
}  // namespace base
