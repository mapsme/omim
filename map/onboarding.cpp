#include "map/onboarding.hpp"

#include "partners_api/utm.hpp"

#include "platform/platform.hpp"
#include "platform/preferred_languages.hpp"

#include "base/url_helpers.hpp"

#include <array>
#include <cstdint>
#include <ctime>

#include "private.h"

namespace
{
auto constexpr kTipsCount = static_cast<uint8_t>(onboarding::Tip::Type::Count);

std::string const kBaseUrl = "/v2/mobilefront/";
std::array<std::string, kTipsCount> const kTipsLinks = {
  InjectUTM(kBaseUrl, UTM::DiscoverCatalogOnboarding),
  InjectUTM(kBaseUrl + "search/?tag=181", UTM::FreeSamplesOnboading),
  ""};
}  // namespace

namespace onboarding
{
Tip GetTip()
{
  auto const tipIndex = std::time(nullptr) % kTipsCount;
  auto const link = kTipsLinks[tipIndex];
  auto const catalogUrl = base::url::Join(BOOKMARKS_CATALOG_FRONT_URL, languages::GetCurrentNorm());
  return {static_cast<Tip::Type>(tipIndex),
          link.empty() ? link : base::url::Join(catalogUrl, link)};
}

bool CanShowTipButton() { return GetPlatform().IsConnected(); }
}  // namespace onboarding
