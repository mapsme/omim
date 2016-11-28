#pragma once

#include "std/function.hpp"
#include "std/string.hpp"

namespace platform
{
/// Possible user answer which returned from platform in UserChoiceCallback parameter.
enum class InetAskResult
{
  None,
  Ask,  // ask every day
  Always,
  Never
};

inline string ToString(InetAskResult const askResult)
{
  switch (askResult)
  {
  case InetAskResult::None: return "None";
  case InetAskResult::Ask: return "Ask";
  case InetAskResult::Always: return "Always";
  case InetAskResult::Never: return "Never";
  }
}

inline string DebugPrint(platform::InetAskResult const askResult) { return ToString(askResult); }

using UserChoiceCallback = function<void(InetAskResult askResult)>;

/// Method which called when we need to ask user about using mobile data.
/// @cb - callback which must be called when the user makes a choice.
void UseMobileDataAsk(UserChoiceCallback cb);
}  // namespace platform
