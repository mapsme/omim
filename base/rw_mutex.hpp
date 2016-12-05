#pragma once

#include "base/assert.hpp"

#include "std/mutex.hpp"

namespace threads
{
class RWMutex
{
public:
  RWMutex() : m_counter(0) {}
  void LockRead()
  {
    std::lock_guard<mutex> lockWriter(m_noWritersMutex);
    {
      std::lock_guard<mutex> lockCounter(m_counterMutex);
      size_t const prevCounter = m_counter;
      m_counter++;
      if (prevCounter == 0)
        m_noReadersMutex.lock();
    }
  }

  void UnlockRead()
  {
    std::lock_guard<mutex> lockCounter(m_counterMutex);
    ASSERT_GREATER(m_counter, 0, ());
    m_counter--;
    if (m_counter == 0)
      m_noReadersMutex.unlock();
  }

  void LockWrite()
  {
    std::lock_guard<mutex> lockWriter(m_noWritersMutex);
    m_noReadersMutex.lock();
  }

  void UnlockWrite() { m_noReadersMutex.unlock(); }
private:
  mutex m_noWritersMutex;
  mutex m_noReadersMutex;
  mutex m_counterMutex;
  size_t m_counter;
};

class ReadLockGuard
{
public:
  ReadLockGuard(RWMutex & mutex) : m_mutex(mutex) { m_mutex.LockRead(); }
  ~ReadLockGuard() { m_mutex.UnlockRead(); }
private:
  RWMutex & m_mutex;
};

class WriteLockGuard
{
public:
  WriteLockGuard(RWMutex & mutex) : m_mutex(mutex) { m_mutex.LockWrite(); }
  ~WriteLockGuard() { m_mutex.UnlockWrite(); }
private:
  RWMutex & m_mutex;
};
}  //  namespace threads
