#include "testing/testing.hpp"

#include "partners_api/google_ads.hpp"

UNIT_TEST(Google_BannerInSearch)
{
  ads::Google google;
  TEST(google.HasBanner(), ());
  auto result = google.GetBanner();
  TEST_EQUAL(result, "dummy", ());
}
