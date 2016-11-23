#include "partners_api/remote_call_guard.hpp"

#include "platform/settings.hpp"

#include "base/string_utils.hpp"

using namespace platform;

namespace
{
auto const kOneDay = 24 * 60 * 60;
}  // namespace

bool RemoteCallGuard::IsAskRequired()
{
  if (m_lastResult == InetAskResult::None)
    return true;

  if (m_lastResult != InetAskResult::Ask)
    return false;

  if (m_askedTime == 0)
    return true;

  auto const askedTime = time(nullptr);
  if (askedTime - m_askedTime > kOneDay)
    return true;

  return false;
}

void RemoteCallGuard::LoadAskResult()
{
  if (m_lastResult != InetAskResult::None)
    return;

  string useMobileData;
  if (!settings::Get("UseMobileData", useMobileData))
    return;

  if (useMobileData == "Ask")
    m_lastResult = InetAskResult::Ask;
  else if (useMobileData == "Always")
    m_lastResult = InetAskResult::Always;
  else if (useMobileData == "Never")
    m_lastResult = InetAskResult::Never;
  else
    CHECK(false, ("Incorrect value for setting UseMobileData"));
}

void RemoteCallGuard::LoadAskedTime()
{
  if (m_askedTime != 0)
    return;

  string askedTime;
  if (!settings::Get("UseMobileDataAskedTime", askedTime))
    return;

  CHECK(!askedTime.empty(), ());
  CHECK(isdigit(askedTime[0]), ());

  strings::to_uint64(askedTime, m_askedTime);
}

bool RemoteCallGuard::NeedToCallRemote()
{
  if (GetPlatform().ConnectionStatus() == Platform::EConnectionType::CONNECTION_WIFI)
    return true;

  if (GetPlatform().InRoaming() || m_lastResult == InetAskResult::None ||
      m_lastResult == InetAskResult::Never)
    return false;

  return true;
}

void RemoteCallGuard::SaveUserChoice(InetAskResult askResult)
{
  settings::Set("UseMobileData", ToString(askResult));
  m_lastResult = askResult;

  if (askResult == InetAskResult::Ask)
  {
    auto const askedTime = time(nullptr);
    settings::Set("UseMobileDataAskedTime", strings::to_string(askedTime));
    m_askedTime = askedTime;
  }
}
