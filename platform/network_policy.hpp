#pragma once

#include <functional>

class _JNIEnv;
class _jobject;
typedef _JNIEnv JNIEnv;
typedef _jobject * jobject;

namespace platform
{
class NetworkPolicy;
using PartnersApiFn = std::function<void(NetworkPolicy const & canUseNetwork)>;
}

namespace network_policy
{
void CallPartnersApi(platform::PartnersApiFn fn, bool force);
}

namespace platform
{
/// Class that is used to allow or disallow remote calls.
class NetworkPolicy
{
  // Maker for android.
  friend NetworkPolicy ToNativeNetworkPolicy(JNIEnv * env, jobject obj);

  // iOS
  friend void network_policy::CallPartnersApi(platform::PartnersApiFn fn, bool force);

public:
  enum class Stage
  {
    Always,
    Session,
    Never
  };

  bool CanUse() const { return m_canUse; }

private:
  NetworkPolicy(bool const canUseNetwork) : m_canUse(canUseNetwork) {}

  bool m_canUse = false;
};
}  // namespace platform
