#pragma once

#include "platform/mobile_data_ask.hpp"
#include "platform/platform.hpp"

/// Class that is used when we need to ensure the possibility to use mobile data.
class RemoteCallGuard
{
public:
  static RemoteCallGuard & Instance()
  {
    static RemoteCallGuard instance;
    return instance;
  }

  template <typename RemoteCallFunc>
  void Call(RemoteCallFunc && remoteCallFunc)
  {
    using platform::InetAskResult;
    // When connection type is equal to Wi-Fi - load online data without questions.
    if (GetPlatform().ConnectionStatus() == Platform::EConnectionType::CONNECTION_WIFI)
      remoteCallFunc();

    // Never load online data by using mobile internet in roaming.
    if (GetPlatform().InRoaming())
      return;

    LoadAskResult();
    LoadAskedTime();

    auto const askCallback = [this, remoteCallFunc](InetAskResult askResult)
    {
      SaveUserChoice(askResult);
      if (NeedToCallRemote())
        remoteCallFunc();
    };

    if (IsAskRequired())
      platform::UseMobileDataAsk(askCallback);
    else if (NeedToCallRemote())
      remoteCallFunc();
  }

private:
  bool IsAskRequired();
  void LoadAskResult();
  void LoadAskedTime();
  bool NeedToCallRemote();
  void SaveUserChoice(platform::InetAskResult askResult);

  platform::InetAskResult m_lastResult = platform::InetAskResult::None;
  uint64_t m_askedTime = 0;
};
