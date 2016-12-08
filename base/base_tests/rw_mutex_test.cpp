#include "testing/testing.hpp"

#include "base/logging.hpp"
#include "base/rw_mutex.hpp"

#include "std/chrono.hpp"
#include "std/thread.hpp"

UNIT_TEST(RwMutexTest)
{
  size_t value = 0;
  size_t maxReadValue1 = 0;
  size_t maxReadValue2 = 0;
  threads::RWMutex mutex;

  std::thread readThread([&value, &maxReadValue1, &mutex]() {
    for (size_t i = 0; i < 25; i++)
    {
      this_thread::sleep_for(milliseconds(1));
      threads::ReadLockGuard lock(mutex);
      if (value > maxReadValue1)
        maxReadValue1 = value;
    }
  });

  std::thread readThread2([&value, &maxReadValue2, &mutex]() {
    for (size_t i = 0; i < 10; i++)
    {
      this_thread::sleep_for(milliseconds(4));
      threads::ReadLockGuard lock(mutex);
      if (value > maxReadValue2)
        maxReadValue2 = value;
    }
  });

  std::thread writeThread([&value, &mutex]() {
    for (size_t i = 0; i < 3; i++)
    {
      this_thread::sleep_for(milliseconds(10));
      threads::WriteLockGuard lock(mutex);
      value++;
    }
  });

  readThread.join();
  readThread2.join();
  writeThread.join();

  TEST_EQUAL(maxReadValue1, 2, ());
  TEST_EQUAL(maxReadValue2, 3, ());
}
