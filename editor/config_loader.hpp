#pragma once

#include "base/exception.hpp"
#include "base/logging.hpp"

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <memory>
#include <string>
#include <thread>

namespace pugi
{
class xml_document;
}

namespace editor
{
class EditorConfigWrapper;

// Class for multithreaded interruptable waiting.
class Waiter
{
public:
  template <typename Rep, typename Period>
  bool Wait(std::chrono::duration<Rep, Period> const & waitDuration)
  {
    std::unique_lock<std::mutex> lock(m_mutex);

    if (m_interrupted)
      return false;

    m_event.wait_for(lock, waitDuration, [this]() { return m_interrupted; });

    return true;
  }

  void Interrupt();

private:
  bool m_interrupted = false;
  std::mutex m_mutex;
  std::condition_variable m_event;
};

// Class which loads config from local drive, checks hash
// for config on server and downloads new config if needed.
class ConfigLoader
{
public:
  explicit ConfigLoader(EditorConfigWrapper & config);
  ~ConfigLoader();

  // Static methods for production and testing.
  static void LoadFromLocal(pugi::xml_document & doc);
  static std::string GetRemoteHash();
  static void GetRemoteConfig(pugi::xml_document & doc);
  static bool SaveHash(std::string const & hash, std::string const & filePath);
  static std::string LoadHash(std::string const & filePath);

private:
  void LoadFromServer();
  bool SaveAndReload(pugi::xml_document const & doc);
  void ResetConfig(pugi::xml_document const & doc);

  EditorConfigWrapper & m_config;

  Waiter m_waiter;
  std::thread m_loaderThread;
};
}  // namespace editor
