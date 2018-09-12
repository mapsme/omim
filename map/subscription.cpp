#include "map/subscription.hpp"

#include "platform/http_client.hpp"
#include "platform/platform.hpp"

#include "coding/serdes_json.hpp"
#include "coding/sha1.hpp"
#include "coding/writer.hpp"

#include "base/assert.hpp"
#include "base/logging.hpp"
#include "base/visitor.hpp"

#include "std/target_os.hpp"

#include <utility>

#define STAGE_PURCHASE_SERVER
#include "private.h"

namespace
{
std::string const kSubscriptionId = "SubscriptionId";
std::string const kServerUrl = PURCHASE_SERVER_URL;
#if defined(OMIM_OS_IPHONE)
std::string const kProductId = "ad.removal.apple";
std::string const kReceiptType = "apple";
#elif defined(OMIM_OS_ANDROID)
std::string const kProductId = "ad.removal.google";
std::string const kReceiptType = "google";
#else
std::string const kProductId {};
std::string const kReceiptType {};
#endif

std::string GetSubscriptionId()
{
  return coding::SHA1::CalculateBase64ForString(GetPlatform().UniqueClientId());
}

std::string ValidationUrl()
{
  if (kServerUrl.empty())
    return {};
  return kServerUrl + "purchases/is_purchased";
}

struct ReceiptData
{
  ReceiptData(std::string const & data, std::string const & type)
    : m_data(data)
    , m_type(type)
  {}

  std::string m_data;
  std::string m_type;

  DECLARE_VISITOR(visitor(m_data, "data"),
                  visitor(m_type, "type"))
};

struct ValidationData
{
  ValidationData(std::string const & productId, std::string const & receiptData,
                 std::string const & receiptType)
    : m_productId(productId)
    , m_receipt(receiptData, receiptType)
  {}

  std::string m_productId;
  ReceiptData m_receipt;

  DECLARE_VISITOR(visitor(m_productId, "product_id"),
                  visitor(m_receipt, "receipt"))
};

struct ValidationResult
{
  bool m_isValid = true;
  std::string m_error;

  DECLARE_VISITOR(visitor(m_isValid, "valid"),
                  visitor(m_error, "error"))
};
}  // namespace

Subscription::Subscription()
{
  std::string id;
  if (GetPlatform().GetSecureStorage().Load(kSubscriptionId, id))
    m_subscriptionId = id;
  m_isActive = (GetSubscriptionId() == m_subscriptionId);
}

void Subscription::Register(SubscriptionListener * listener)
{
  CHECK(listener != nullptr, ());
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  m_listeners.emplace_back(listener);
  listener->OnSubscriptionChanged(IsActive());
}

void Subscription::SetValidationCallback(ValidationCallback && callback)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  m_validationCallback = std::move(callback);
}

bool Subscription::IsActive() const
{
  return m_isActive;
}

void Subscription::Validate(std::string const & receiptData, std::string const & accessToken)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());

  std::string const url = ValidationUrl();
  if (url.empty())
  {
    ApplyValidation(ValidationCode::NotActive);
    return;
  }

  // If we validate the subscription immediately after purchasing, we believe that
  // the subscription is valid and apply it before the validation. If the result of
  // validation will be different, we return everything back.
  bool isJustPaidSubscription = m_subscriptionId.empty() && !receiptData.empty();
  if (isJustPaidSubscription)
  {
    m_isActive = true;
    m_subscriptionId = GetSubscriptionId();
    GetPlatform().GetSecureStorage().Save(kSubscriptionId, m_subscriptionId);
    for (auto & listener : m_listeners)
      listener->OnSubscriptionChanged(true /* isActive */);
  }

  auto const status = GetPlatform().ConnectionStatus();
  if (status == Platform::EConnectionType::CONNECTION_NONE || receiptData.empty())
  {
    ApplyValidation(ValidationCode::Failure);
    return;
  }

  if (!isJustPaidSubscription && status != Platform::EConnectionType::CONNECTION_WIFI)
  {
    ApplyValidation(ValidationCode::Failure);
    return;
  }

  GetPlatform().RunTask(Platform::Thread::Network, [this, url, receiptData, accessToken]()
  {
    platform::HttpClient request(url);
    request.SetRawHeader("Accept", "application/json");
    request.SetRawHeader("User-Agent", GetPlatform().GetAppUserAgent());
    if (!accessToken.empty())
      request.SetRawHeader("Authorization", "Bearer " + accessToken);

    std::string jsonStr;
    {
      using Sink = MemWriter<std::string>;
      Sink sink(jsonStr);
      coding::SerializerJson<Sink> serializer(sink);
      serializer(ValidationData(kProductId, receiptData, kReceiptType));
    }
    request.SetBodyData(std::move(jsonStr), "application/json");

    ValidationCode code = ValidationCode::Failure;
    if (request.RunHttpRequest())
    {
      auto const resultCode = request.ErrorCode();
      if (resultCode >= 200 && resultCode < 300)
      {
        ValidationResult result;
        {
          coding::DeserializerJson deserializer(request.ServerResponse());
          deserializer(result);
        }

        code = result.m_isValid ? ValidationCode::Active : ValidationCode::NotActive;
        if (!result.m_error.empty())
          LOG(LWARNING, ("Validation URL:", url, "Subscription error:", result.m_error));
      }
      else
      {
        LOG(LWARNING, ("Validation URL:", url, "Unexpected server error. Code =", resultCode, request.ServerResponse()));
      }
    }
    else
    {
      LOG(LWARNING, ("Validation URL:", url, "Request failed."));
    }

    GetPlatform().RunTask(Platform::Thread::Gui, [this, code]() { ApplyValidation(code); });
  });
}

void Subscription::ApplyValidation(ValidationCode code)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());

  if (code != ValidationCode::Failure)
  {
    bool const isActive = (code == ValidationCode::Active);
    m_isActive = isActive;
    if (isActive)
    {
      m_subscriptionId = GetSubscriptionId();
      GetPlatform().GetSecureStorage().Save(kSubscriptionId, m_subscriptionId);
    }
    else
    {
      GetPlatform().GetSecureStorage().Remove(kSubscriptionId);
    }

    for (auto & listener : m_listeners)
      listener->OnSubscriptionChanged(isActive);
  }

  if (m_validationCallback)
    m_validationCallback(code);
}
